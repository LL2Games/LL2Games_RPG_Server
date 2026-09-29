#include "MapInstance.h"
#include "PlayerHandler.h"
#include "ChannelPacketFactory.h"
#include "PacketParser.h"
#include "GameplayGate.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

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
}
int main() {
    Gameplay::Guard guard(Gameplay::gate);
    try {
        TestPacketsAndTick(); TestPlayerPhysics(); TestMonsterPhysics(); TestMonsterSnapshot();
        std::cout << "Movement pipeline checks passed: " << checks << '\n';
    } catch(const std::exception& e) { std::cerr << "[FAIL] " << e.what() << '\n'; return 1; }
}
