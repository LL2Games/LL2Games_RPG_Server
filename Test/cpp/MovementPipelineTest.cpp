#include "MapInstance.h"
#include "PlayerHandler.h"
#include "ChannelPacketFactory.h"
#include "PacketParser.h"
#include "GameplayGate.h"
#include "LevelManager.h"
#include "MapService.h"
#include "MapManager.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

// DB 연결 없이 실제 경험치 감소 공식을 검증하는 테스트 데이터.
class LevelManagerTestAccess {
public:
    static void Seed(int level, int64_t needExp) {
        LevelManager::GetInstance()->m_needExpTable[level] = needExp;
    }
};

namespace {
int checks = 0;
void Check(bool ok, const char* text) {
    if (!ok) throw std::runtime_error(text);
    ++checks;
    std::cout << "[PASS] " << text << '\n';
}
bool Near(float a, float b) { return std::fabs(a-b) < 0.02f; }
struct PacketRow { int type; std::vector<std::string> fields; };
struct Peer {
    int fd[2]{-1,-1};
    ChannelSession session;
    static int Open(int (&pair)[2]) {
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, pair) != 0)
            throw std::runtime_error("socketpair");
        return pair[0];
    }
    Peer() : session(Open(fd), nullptr, 1, 1) {}
    ~Peer() { session.SetPlayer(nullptr); close(fd[0]); close(fd[1]); }
    std::vector<PacketRow> Read() {
        Check(session.FlushSend(), "송신 큐 flush");
        std::vector<char> bytes;
        char buffer[8192];
        for (;;) {
            const auto n = recv(fd[1], buffer, sizeof(buffer), 0);
            if (n > 0) bytes.insert(bytes.end(), buffer, buffer+n);
            else if (n < 0 && errno == EINTR) continue;
            else break;
        }
        std::vector<PacketRow> rows;
        while (!bytes.empty()) {
            auto result = PacketParser::TryParse(bytes);
            if (result.status != ParseStatus::Complete) throw std::runtime_error("incomplete test packet");
            PacketRow row{result.packet.type,{}};
            size_t offset=0;
            while(offset<result.packet.payload.size()) {
                std::string value,error;
                if(!PacketParser::ParseLengthPrefixedString(result.packet.payload.data(),
                    result.packet.payload.size(),offset,value,error)) throw std::runtime_error(error);
                row.fields.push_back(value);
            }
            rows.push_back(std::move(row));
        }
        return rows;
    }
};
void InitPlayer(Player& player, int id, float x) {
    PlayerInitData init{id,"pipeline","test",1,0,20000,42,x,90};
    CharacterStat stat{BaseStat{4,4,4,4},DerivedStat{100,50},ExpStat{1,0,100},100,50,0};
    player.SetInitData(init,stat);
}
MapInitData MapData() {
    MapInitData data{};
    data.mapID=42;
    data.physics.minX=-1000; data.physics.maxX=1000; data.physics.killY=500;
    data.physics.safeFeet={0,100};
    data.physics.platforms={{1,-1000,1000,100},{2,100,200,-100}};
    data.physics.climbables={{1,Movement::ClimbKind::Ladder,150,-100,100,16,2}};
    return data;
}
struct Scene {
    MapInstance map;
    Player player, observer;
    Peer selfPeer, otherPeer;
    Scene() {
        Check(map.Init(MapData())==1,"빈 테스트 맵 초기화");
        InitPlayer(player,1,0); InitPlayer(observer,2,-700);
        player.SetCurrentMap(&map); observer.SetCurrentMap(&map);
        player.SetSession(&selfPeer.session); observer.SetSession(&otherPeer.session);
        selfPeer.session.SetPlayer(&player); otherPeer.session.SetPlayer(&observer);
        map.OnEnter(1,&player); map.OnEnter(2,&observer);
    }
    ~Scene() {
        map.OnLeave(1); map.OnLeave(2);
        selfPeer.session.SetPlayer(nullptr); otherPeer.session.SetPlayer(nullptr);
        player.SetSession(nullptr); observer.SetSession(nullptr);
    }
    void Packet(int type, const std::vector<std::string>& fields, bool truncate=false) {
        auto body=PacketParser::MakeBody(fields);
        if(truncate && !body.empty()) body.pop_back();
        PacketContext ctx{};
        ctx.type=type; ctx.channel_session=&selfPeer.session;
        ctx.payload=body.data(); ctx.payload_len=static_cast<int>(body.size());
        ChannelPacketFactory factory;
        auto handler=factory.Create(type);
        Check(static_cast<bool>(handler),"패킷 팩토리 핸들러 연결");
        handler->Execute(&ctx);
    }
    void Input(int seq,int x,int y=0,int jump=0) {
        Packet(PKT_MOVEMENT_INPUT,{"42",std::to_string(player.MovementEpoch()),
            std::to_string(seq),std::to_string(x),std::to_string(y),std::to_string(jump)});
    }
    void Step(float dt) { map.Update(dt); selfPeer.Read(); otherPeer.Read(); }
};
const PacketRow& Find(const std::vector<PacketRow>& rows,int kind,int id) {
    for(const auto& row:rows)
        if(row.type==PKT_MOVEMENT_SNAPSHOT && row.fields.size()==16 &&
           row.fields[1]==std::to_string(kind) && row.fields[2]==std::to_string(id)) return row;
    throw std::runtime_error("snapshot missing");
}
void TestPacketsAndTick() {
    Scene scene;
    Check(scene.player.MovementBody().mode==Movement::Mode::Grounded,"입장 때 발판 판정 및 epoch 초기화");
    scene.map.SendEnterPackets(&scene.player);
    auto initial=scene.selfPeer.Read(); scene.otherPeer.Read();
    Check(Find(initial,0,1).fields[3]==std::to_string(scene.player.MovementEpoch()),"입장 스냅샷에서 내 epoch 수신");
    scene.Input(1,1);
    Check(Near(scene.player.GetPos().xPos,0),"입력 수신만으로 좌표 변경 없음");
    scene.map.Update(0.05f);
    auto self=scene.selfPeer.Read(), other=scene.otherPeer.Read();
    const auto& a=Find(self,0,1); const auto& b=Find(other,0,1);
    Check(a.fields==b.fields,"본인과 관찰자에게 동일한 16필드 스냅샷 전송");
    Check(a.fields[4]=="3" && a.fields[5]=="1","50ms는 물리 3회, 입력 순번 반영");
    Check(Near(scene.player.GetPos().xPos,10) && Near(scene.player.GetPos().yPos,90),"서버 속도 200으로 50ms 동안 10 이동");
    const int epoch=scene.player.MovementEpoch();
    auto fields=std::vector<std::string>{"42",std::to_string(epoch),"2","1","0","0"};
    fields.push_back("extra"); scene.Packet(PKT_MOVEMENT_INPUT,fields);
    fields.pop_back(); scene.Packet(PKT_MOVEMENT_INPUT,fields,true);
    fields[2]="2x"; scene.Packet(PKT_MOVEMENT_INPUT,fields);
    fields[2]="2147483648"; scene.Packet(PKT_MOVEMENT_INPUT,fields);
    fields[2]="2"; fields[0]="43"; scene.Packet(PKT_MOVEMENT_INPUT,fields);
    fields[0]="42"; fields[5]="2"; scene.Packet(PKT_MOVEMENT_INPUT,fields);
    fields[5]="0"; fields[3]="2"; scene.Packet(PKT_MOVEMENT_INPUT,fields);
    Check(scene.player.LastInputSequence()==1,"필드 초과/누락/잘못된 정수/다른 맵/잘못된 버튼과 축 거부");
    const Vec2 pos=scene.player.GetPos();
    scene.Packet(PKT_PLAYER_MOVE,{"999","999","999","1"});
    Check(Near(scene.player.GetPos().xPos,pos.xPos),"구 좌표 패킷으로 위치 조작 불가");
    auto errors=scene.selfPeer.Read();
    bool rejected=false;
    for(const auto& row:errors) if(row.type==PKT_PLAYER_MOVE && row.fields[0]=="nok") rejected=true;
    Check(rejected,"구 이동 패킷에 명시적 nok 응답");
    scene.Input(2,1); scene.map.Update(1.0f);
    self=scene.selfPeer.Read(); scene.otherPeer.Read();
    Check(Find(self,0,1).fields[4]=="18","긴 지연은 최대 15스텝으로 제한");
    scene.map.Update(std::numeric_limits<float>::quiet_NaN());
    Check(!scene.selfPeer.session.HasPendingSend(),"비정상 dt는 물리 및 송신 중단");
    scene.map.OnLeave(1); scene.Input(3,1);
    Check(scene.player.LastInputSequence()==2,"맵에서 퇴장한 플레이어 입력 거부");
}
void TestPlayerPhysics() {
    Scene scene;
    scene.Input(1,0,0,1); scene.Step(0.05f);
    Check(scene.player.GetPos().yPos<90 && scene.player.MovementBody().vy<0,"실제 점프 상승");
    const float vy=scene.player.MovementBody().vy;
    scene.Input(2,0,0,1); scene.Step(0.05f);
    Check(scene.player.MovementBody().vy>vy,"공중 재점프는 속도 재설정하지 않음");
    for(int i=0;i<30;++i) scene.Step(0.05f);
    Check(Near(scene.player.GetPos().yPos,90) && scene.player.MovementBody().mode==Movement::Mode::Grounded,"낙하 후 정확한 발 높이로 착지");
    scene.player.SetPos({150,90}); scene.map.ResetPlayerMovement(&scene.player,false);
    scene.Input(1,0,-1); scene.Step(0.05f);
    Check(scene.player.MovementBody().mode==Movement::Mode::Climbing,"사다리 탑승");
    scene.Input(2,0,0); scene.Step(0.05f);
    const float ladderY=scene.player.GetPos().yPos; scene.Step(0.05f);
    Check(Near(scene.player.GetPos().yPos,ladderY),"사다리 정지 중 중력 미적용");
    scene.Input(3,1,0,1); scene.Step(0.05f);
    Check(scene.player.MovementBody().climbableId==0 && scene.player.MovementBody().mode==Movement::Mode::Rising,"사다리 점프 이탈");
    scene.player.SetPos({0,501});
    const int epoch=scene.player.MovementEpoch(); scene.Step(0.05f);
    Check(scene.player.MovementEpoch()==epoch+1 && Near(scene.player.GetPos().yPos,90),"맵 밖 추락 시 안전 지점과 새 epoch");
    Check(scene.player.GetCurHP()==100,"추락 복귀는 HP를 변경하지 않음");
    scene.Input(1,1); scene.Step(0.2f); scene.Step(0.2f);
    const float stoppedX=scene.player.GetPos().xPos; scene.Step(0.05f);
    Check(Near(scene.player.GetPos().xPos,stoppedX),"입력 중단 후 수평 이동 정지");
    scene.player.SetState(PlayerState::DEAD); scene.Input(2,1); scene.Step(0.05f);
    Check(Near(scene.player.GetPos().xPos,stoppedX),"죽은 플레이어 물리 이동 생략");
}
Monster MakeMonster(MapInstance& map,bool avoid,bool jump,float x=99,float y=90) {
    MonsterTemplate t{};
    t.monsterId=100101; t.name="test"; t.hp=100; t.level=1; t.moveSpeed=200;
    t.mapId=42; t.mapInstance=&map; t.collisionType=ColliderType::Rect2D;
    t.offset={0,0}; t.half={10,10}; t.avoidCliff=avoid; t.canJump=jump;
    MonsterSpawnData spawn{}; spawn.spawnPos={x,y}; spawn.instanceId=1;
    Monster m; Check(m.Init(t,spawn)==1,"몬스터 물리 초기화"); return m;
}
void TestMonsterPhysics() {
    MapInstance map;
    auto data=MapData(); data.physics.platforms={{1,0,100,100}}; data.physics.climbables.clear();
    Check(map.Init(data)==1,"절벽 테스트 맵 초기화");
    auto walker=MakeMonster(map,true,false);
    walker.Update(Movement::StepSeconds);
    Check(Near(walker.GetPos().xPos,99) && walker.GetDir().xPos==-1,"절벽 회피 몬스터는 멈추고 방향 전환");
    auto jumper=MakeMonster(map,true,true); jumper.Update(Movement::StepSeconds);
    Check(jumper.MovementBody().mode==Movement::Mode::Rising,"점프 가능 몬스터는 절벽에서 점프");
    auto faller=MakeMonster(map,false,false); faller.Update(Movement::StepSeconds);
    Check(faller.MovementBody().mode==Movement::Mode::Falling,"절벽 회피 해제 몬스터는 낙하");
    Player target; InitPlayer(target,7,60); target.SetPos({60,-300}); target.SetCurrentMap(&map);
    auto chaser=MakeMonster(map,false,false,40,90);
    chaser.OnDamaged(&target,1); chaser.Update(Movement::StepSeconds);
    Check(chaser.GetPos().xPos>40 && Near(chaser.GetPos().yPos,90),"위쪽 플레이어 추적 시 Y좌표 직접 이동 금지");
    const int hp=chaser.GetCurrentHP(), epoch=chaser.MovementEpoch();
    chaser.SetPos({200,501}); chaser.Update(Movement::StepSeconds);
    Check(chaser.MovementEpoch()==epoch+1 && Near(chaser.GetPos().xPos,40) && chaser.GetCurrentHP()==hp,"몬스터 추락은 스폰 복귀, HP 유지");
    chaser.Dead(); const auto deadPos=chaser.GetPos(); chaser.Update(Movement::StepSeconds);
    Check(Near(chaser.GetPos().xPos,deadPos.xPos) && chaser.MovementBody().vx==0,"죽은 몬스터 정지");
    chaser.Reset();
    Check(chaser.IsAlive() && chaser.MovementEpoch()==epoch+2 && chaser.GetCurrentHP()==100,"몬스터 리스폰에서 HP/물리/epoch 복구");
    map.OnEnter(7,&target);
    map.GetMonsterList().push_back(MakeMonster(map,true,false));
    auto& tracked=map.GetMonsterList().back(); tracked.OnDamaged(&target,1);
    map.OnLeave(7);
    Check(tracked.GetLastAttacker()==nullptr && tracked.GetState()==E_Patrol,"퇴장 시 추적 대상 포인터 제거");
}
void TestMonsterSnapshot() {
    Scene scene;
    scene.map.GetMonsterList().push_back(MakeMonster(scene.map,true,false,500,90));
    scene.map.BroadcastMovement();
    auto own=scene.selfPeer.Read(), other=scene.otherPeer.Read();
    Check(Find(own,1,1).fields==Find(other,1,1).fields,"몬스터 스냅샷도 본인/관찰자 일치");
    Check(Find(own,1,1).fields[14]=="100","몬스터 HP 포함");
}
void TestDeathReviveLifecycle() {
    Scene scene;
    LevelManagerTestAccess::Seed(1,100);
    scene.player.GetStat() = CharacterStat{BaseStat{4,4,4,4},DerivedStat{100,50},
        ExpStat{1,80,100},100,50,0};
    const int aliveEpoch=scene.player.MovementEpoch();
    scene.Input(1,1,0,1);
    auto& body=scene.player.MovementBody();
    body.vx=200; body.vy=-100; body.mode=Movement::Mode::Climbing; body.climbableId=1;
    scene.player.OnDamaged(0,1000);
    scene.player.OnDamaged(-10,1000);
    Check(scene.player.GetCurHP()==100,"0/음수 피해는 무시");
    scene.player.OnDamaged(200,1000);
    Check(!scene.player.IsAlive() && scene.player.GetCurHP()==0,"치명타에서 DEAD 및 HP 0");
    Check(scene.player.GetStatSnapShot().GetExp()==70,"사망 시 필요 경험치 10%를 한 번 감소");
    Check(scene.player.MovementEpoch()==aliveEpoch+1 && body.vx==0 && body.vy==0 &&
        body.climbableId==0 && scene.player.LastInputSequence()==0,"사망 시 속도/탑승/순번 해제 및 새 epoch");
    const auto deadInput=scene.player.ConsumeMovement(0);
    Check(deadInput.horizontal==0 && !deadInput.jump,"사망 시 남은 방향과 점프 해제");
    const auto deadVersion=scene.player.MakeSaveData().saveVersion;
    scene.player.OnDamaged(200,1001); scene.player.Dead();
    Check(scene.player.GetStatSnapShot().GetExp()==70 &&
        scene.player.MakeSaveData().saveVersion==deadVersion &&
        scene.player.MovementEpoch()==aliveEpoch+1,"중복 피격/Dead 호출은 경험치와 상태를 다시 변경하지 않음");
    Check(!scene.player.AcceptMovement(aliveEpoch,2,{1,0,true}),"사망 전 입력 거부");
    Check(!scene.player.Revive({std::numeric_limits<float>::quiet_NaN(),0}),"잘못된 부활 좌표 거부");
    scene.Packet(PKT_PLAYER_REVIVE,{"unexpected"}); scene.selfPeer.Read();
    Check(!scene.player.IsAlive(),"부활 요청은 빈 payload만 허용");
    scene.Packet(PKT_PLAYER_REVIVE,{});
    auto own=scene.selfPeer.Read(), other=scene.otherPeer.Read();
    const auto& revived=Find(own,0,1);
    Check(revived.fields==Find(other,0,1).fields,"부활 위치/HP/epoch를 본인과 관찰자에게 동일하게 전달");
    Check(revived.fields[14]=="100" && revived.fields[13]==std::to_string(static_cast<int>(PlayerState::IDLE)),
        "부활 스냅샷에 살아 있는 상태와 최대 HP 반영");
    Check(scene.player.MovementEpoch()==aliveEpoch+2 && Near(scene.player.GetPos().xPos,0) &&
        Near(scene.player.GetPos().yPos,90) && body.mode==Movement::Mode::Grounded,
        "부활 시 안전 발판 원점 및 epoch 한 번 갱신");
    Check(scene.player.IsSaveNeeded() && scene.player.IsStatDirty(),"사망/부활 결과 저장 및 스탯 전송 필요 표시");
    bool ok=false;
    for(const auto& row:own) if(row.type==PKT_PLAYER_REVIVE && row.fields[0]=="ok") ok=true;
    Check(ok,"성공한 부활에만 ok 응답");
    const auto revivedVersion=scene.player.MakeSaveData().saveVersion;
    scene.Packet(PKT_PLAYER_REVIVE,{}); own=scene.selfPeer.Read();
    Check(own.size()==1 && own[0].type==PKT_PLAYER_REVIVE && own[0].fields[0]=="nok" &&
        scene.player.MakeSaveData().saveVersion==revivedVersion,"살아 있는 상태의 중복 부활은 nok, 상태 변경 없음");
    Check(!scene.player.AcceptMovement(aliveEpoch+1,2,{1,0,true}),"부활 이전 epoch 입력 거부");
    scene.map.Update(0.05f); scene.selfPeer.Read(); scene.otherPeer.Read();
    Check(Near(scene.player.GetPos().xPos,0) && Near(scene.player.GetPos().yPos,90),"부활 직후 이전 입력/속도 때문에 움직이지 않음");
    scene.Input(1,1); scene.Step(0.05f);
    Check(scene.player.GetPos().xPos>0,"부활 이후 새 입력으로 정상 이동");
    scene.player.Dead(); scene.map.OnLeave(1);
    const int leftEpoch=scene.player.MovementEpoch();
    Check(!scene.map.RevivePlayer(&scene.player) && scene.player.MovementEpoch()==leftEpoch,
        "현재 맵 포인터가 남아 있어도 퇴장한 플레이어는 부활 불가");
}

void TestActionRestrictionsAndMapEntry() {
    Scene scene;
    SkillDef skill{}; skill.category=SkillCategory::BASIC_ATTACK;
    Check(scene.player.CanUseSkill(&skill),"지상 기본 공격 허용");
    scene.player.MovementBody().mode=Movement::Mode::Rising;
    Check(scene.player.CanUseSkill(&skill),"공중 공격 허용 정책 유지");
    scene.player.MovementBody().mode=Movement::Mode::Climbing;
    Check(!scene.player.CanUseSkill(&skill),"줄타기 중 공격 차단");
    scene.player.Dead();
    Check(!scene.player.CanUseSkill(&skill),"사망 중 공격 차단");
    PlayerManager players;
    MapManager maps(nullptr);
    MapService service(players,maps);
    const auto result=service.MoveByPortal(&scene.player,"any");
    Check(!result.success && result.error=="dead player cannot use portal","사망 중 포탈 사용 차단");
    scene.map.RevivePlayer(&scene.player); scene.selfPeer.Read(); scene.otherPeer.Read();
    scene.player.SetState(PlayerState::STUNNED);
    Check(!scene.player.CanUseSkill(&skill),"기존 기절 공격 제한 유지");
    scene.player.SetState(PlayerState::IDLE);
    scene.Input(1,1,0,1);
    const int beforeEpoch=scene.player.MovementEpoch();
    MapInstance destination;
    auto data=MapData(); data.mapID=43;
    Check(destination.Init(data)==1,"목적지 테스트 맵 초기화");
    scene.map.OnLeave(1);
    scene.player.SetMapId(43); scene.player.SetPos({150,90});
    scene.player.SetCurrentMap(&destination); destination.OnEnter(1,&scene.player);
    Check(scene.player.MovementEpoch()==beforeEpoch+1 &&
        !scene.player.AcceptMovement(beforeEpoch,2,{1,0,true}),"맵 변경 시 epoch 갱신 및 이전 입력 거부");
    const auto input=scene.player.ConsumeMovement(0);
    Check(!input.jump && input.horizontal==0 && scene.player.MovementBody().vy==0,
        "맵 변경 시 점프/방향/속도 초기화");
    destination.OnLeave(1);
    scene.player.SetCurrentMap(&scene.map); scene.player.SetMapId(42);
    scene.map.OnEnter(1,&scene.player);
}

}
int main() {
    Gameplay::Guard guard(Gameplay::gate);
    try {
        TestPacketsAndTick(); TestPlayerPhysics(); TestMonsterPhysics(); TestMonsterSnapshot();
        TestDeathReviveLifecycle(); TestActionRestrictionsAndMapEntry();
        std::cout << "Movement pipeline checks passed: " << checks << '\n';
    } catch(const std::exception& e) { std::cerr << "[FAIL] " << e.what() << '\n'; return 1; }
}
