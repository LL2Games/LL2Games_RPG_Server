# Server

# 중력·점프·로프(사다리) 구현 따라 쓰기

> 기준: 2026-09-29 로컬 서버 코드. **이 문서는 구현 안내서이며 게임 소스는 변경하지 않았다.**
> 서버는 실제 파일/함수에 맞춘 C++17 예제다. 클라이언트는 인접 저장소 README의 C++·WinAPI 기준이며, 소스가 없어 클래스명과 연결 지점은 제안이다.
> 한 단계씩 작성 → 빌드 → 확인하고 다음 단계로 넘어간다. 기존 파일 전체를 지우지 말고 표시한 함수/멤버만 변경한다.

## 0. 먼저 정할 규칙

| 항목 | 이번 구현 기준 |
|---|---|
| 좌표 | X는 오른쪽 +, Y는 아래쪽 +, 단위는 게임 좌표/초 |
| 위치 기준 | 기존 객체 원점 유지. 발 위치는 `원점 Y + footOffset` |
| 발판 | 수평 단방향 발판. 위에서 내려올 때 착지하고 아래에서 점프하면 통과 |
| 중력 | 1800, 최대 낙하 속도 1000, 점프 속도 -650 (조정용 초기값) |
| 물리 주기 | 1/60초. 기존 맵 갱신 약 50ms 사이에 여러 번 계산 |
| 권한 | 클라이언트는 입력만 전송, 서버가 위치·착지·탑승 결정 |
| 몬스터 | 같은 물리 사용. 로프 탑승은 하지 않음 |
| 죽음 | 그 자리에서 정지, 입력/속도 초기화. 부활 시 안전 발판으로 이동 |
| 추락 | 플레이어는 맵 안전 지점, 몬스터는 스폰 지점으로 복귀. HP 변화 없음 |
| 상태 | 기존 생명/전투 상태와 새 이동 상태를 분리 |

벽·천장·경사면·이동 발판·아래 점프·낙하 피해는 별도 확장이다. 이 문서의 발판 판정은 그 기능을 포함하지 않는다.

**현재 코드에서 바뀌는 핵심**

- `MovePacket.cpp`: 좌표 수신/`TryApplyMove()` → 입력 수신.
- `Monster::UpdateChase()`: X/Y 직접 이동 → X 이동 의도만 결정.
- `MapInstance::Update()`: 몬스터만 갱신 → 플레이어·몬스터 공통 물리 갱신.
- `PlayerState::DEAD`는 유지한다. `Falling`을 추가한다고 죽음 상태를 덮어쓰면 안 된다.
- `Collider.h`는 전투 판정용이다. 발판 판정은 별도 계산한다.

## 1. 파일 지도

아래 경로는 저장소 루트 기준이다. 이후 `CHANNEL/`은 `SERVER/src/CHANNEL/`을 뜻한다.

| 파일 | 할 일 |
|---|---|
| `CHANNEL/util/MovementPhysics.h` **신규** | 데이터·공통 물리 함수 |
| `CHANNEL/util/MovementMapLoader.h` **신규** | 맵 JSON 검증/로딩 |
| `CHANNEL/util/GameplayGate.h` **신규** | 학습용 게임 상태 직렬화 |
| `SERVER/include/COMMON/CommonEnum.h` | `MapInitData`에 물리 맵 데이터 추가 |
| `CHANNEL/map/MapManager.cpp` | JSON 로딩, 맵 갱신 실행 방식 변경 |
| `CHANNEL/map/MapInstance.h/.cpp` | 고정 주기, 플레이어 물리, 스냅샷 |
| `CHANNEL/domain/Player.h/.cpp` | 물리 상태·입력·초기화·스킬 제한 |
| `CHANNEL/domain/Monster.h/.cpp` | 이동 의도·물리·발판 끝 규칙 |
| `CHANNEL/packet/MovePacket.cpp` | 입력 파싱 |
| `CHANNEL/packet/PlayerHandler.h/.cpp` | 새 입력 핸들러 등록 |
| `CHANNEL/packet/ChannelPacketFactory.cpp` | 새 패킷 연결 |
| `SERVER/include/COMMON/packet/Packet.h` | 입력/물리 스냅샷 opcode |
| `CHANNEL/packet/RevivePacket.cpp` | 실제 부활 실패 전달 |
| `CHANNEL/service/MapService.cpp` | 죽음 중 포탈 제한, 이동 초기화 |
| `CHANNEL/packet/PacketProcessTask.cpp` | 게임 상태 직렬화 진입 |
| `CHANNEL/app/ChannelSession.cpp` | 종료 시 객체 수명 보호 |
| `CHANNEL/data/Maps/*.json` | 발판·사다리·복귀 지점 |
| 클라이언트 입력/수신/렌더링 코드 | 10~11절의 제안 클래스로 연결 |

`MapInstance.h`의 기존 `m_has_player`, `m_destroyRequested`는 선언 시 `= false`로 초기화한다. 빈 맵 상태를 읽기 전에 값이 정해져야 한다.

새 서버 파일은 헤더 전용이므로 이번 예제에서는 Makefile `SRCS` 추가가 없다. `.cpp`로 분리한다면 `CHANNEL/Makefile`에 직접 등록한다.

## 2. 공통 물리 객체부터 작성

**신규: `CHANNEL/util/MovementPhysics.h` — 아래 블록 전체**

`Body`는 속도와 이동 상태만 가진다. 실제 위치는 기존 `Player::GetPos()`와 `Monster::m_Pos`가 계속 보관한다.

```cpp
#pragma once
#include "Math.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Movement {
constexpr float StepSeconds = 1.0f / 60.0f;
constexpr float Gravity = 1800.0f;
constexpr float MaxFall = 1000.0f;
constexpr float JumpSpeed = 650.0f;
constexpr float ClimbSpeed = 180.0f;
constexpr float Epsilon = 0.05f;

enum class Mode : int { Grounded = 0, Rising = 1, Falling = 2, Climbing = 3 };
enum class ClimbKind : int { Rope = 0, Ladder = 1 };

struct Platform {
    int id = 0;
    float left = 0, right = 0, y = 0;
};
struct Climbable {
    int id = 0;
    ClimbKind kind = ClimbKind::Ladder;
    float x = 0, top = 0, bottom = 0, grabRange = 16;
    int topPlatformId = 0; // 최상단에서 올라설 발판 ID
};
struct Map {
    std::vector<Platform> platforms;
    std::vector<Climbable> climbables;
    float minX = 0, maxX = 1600, killY = 1200;
    Vec2 safeFeet{100, 700}; // 객체 원점이 아니라 발 좌표
};
struct Input {
    int horizontal = 0; // -1, 0, 1
    int vertical = 0;   // -1=위, 0, 1=아래
    bool jump = false;  // 누른 순간 한 번만 true
};
struct Body {
    float vx = 0, vy = 0;
    float footOffset = 0;
    float grabCooldown = 0;
    Mode mode = Mode::Falling;
    int platformId = 0;
    int climbableId = 0;
    int facing = 1;
};
struct Result { bool landed = false; bool fellOut = false; };

inline const Platform* FindPlatform(const Map& map, int id) {
    for (const auto& p : map.platforms) if (p.id == id) return &p;
    return nullptr;
}
inline const Climbable* FindClimbable(const Map& map, int id) {
    for (const auto& c : map.climbables) if (c.id == id) return &c;
    return nullptr;
}
inline bool Covers(const Platform& p, float x) {
    return x >= p.left && x <= p.right;
}
inline const Platform* Support(const Map& map, float x, float feetY) {
    for (const auto& p : map.platforms)
        if (Covers(p, x) && std::fabs(feetY - p.y) <= Epsilon) return &p;
    return nullptr;
}
inline void Reset(Body& body) {
    const float offset = body.footOffset;
    const int facing = body.facing;
    body = Body{};
    body.footOffset = offset;
    body.facing = facing;
}
inline void PlaceOnFeet(Vec2& pos, Body& body, const Map& map, Vec2 feet) {
    Reset(body);
    pos = {feet.xPos, feet.yPos - body.footOffset};
    if (const auto* p = Support(map, feet.xPos, feet.yPos)) {
        body.mode = Mode::Grounded;
        body.platformId = p->id;
    }
}

// 초기 위치는 원점 좌표다. 공중이면 그 자리에서 낙하를 시작한다.
inline void ResetAtOrigin(Vec2& pos, Body& body, const Map& map) {
    PlaceOnFeet(pos, body, map, {pos.xPos, pos.yPos + body.footOffset});
}

inline Result Step(Vec2& pos, Body& body, const Map& map,
                   Input input, float moveSpeed, float dt) {
    Result result;
    if (!(dt > 0) || !std::isfinite(dt)) return result;
    input.horizontal = std::clamp(input.horizontal, -1, 1);
    input.vertical = std::clamp(input.vertical, -1, 1);
    body.grabCooldown = std::max(0.0f, body.grabCooldown - dt);
    if (input.horizontal != 0) body.facing = input.horizontal;

    if (body.mode == Mode::Grounded) {
        const auto* support = Support(map, pos.xPos, pos.yPos + body.footOffset);
        if (!support) { body.mode = Mode::Falling; body.platformId = 0; }
    }

    // 점프를 누른 프레임에는 새 사다리를 잡지 않는다.
    if (body.mode != Mode::Climbing && input.vertical != 0 &&
        !input.jump && body.grabCooldown <= 0) {
        const float feetY = pos.yPos + body.footOffset;
        for (const auto& c : map.climbables) {
            if (std::fabs(pos.xPos - c.x) <= c.grabRange &&
                feetY >= c.top - Epsilon && feetY <= c.bottom + Epsilon) {
                body.mode = Mode::Climbing;
                body.climbableId = c.id;
                body.platformId = 0;
                body.vx = body.vy = 0;
                pos.xPos = c.x;
                break;
            }
        }
    }

    if (body.mode == Mode::Climbing) {
        const auto* c = FindClimbable(map, body.climbableId);
        if (!c || input.jump) {
            body.mode = input.jump ? Mode::Rising : Mode::Falling;
            body.vy = input.jump ? -JumpSpeed : 0;
            body.climbableId = 0;
            body.grabCooldown = 0.2f; // 점프로 나가자마자 다시 잡히는 것 방지
        } else {
            pos.xPos = c->x;
            body.vx = 0;
            body.vy = input.vertical * ClimbSpeed;
            float feetY = pos.yPos + body.footOffset + body.vy * dt;
            if (input.vertical < 0 && feetY <= c->top) {
                // 로더가 topPlatformId의 위치를 미리 검증한다.
                const auto* top = FindPlatform(map, c->topPlatformId);
                if (top) {
                    PlaceOnFeet(pos, body, map, {c->x, top->y});
                    body.grabCooldown = 0.2f;
                    return result;
                }
            }
            if (input.vertical > 0 && feetY >= c->bottom) {
                feetY = c->bottom;
                PlaceOnFeet(pos, body, map, {c->x, feetY});
                body.grabCooldown = 0.2f;
                return result;
            }
            pos.yPos = std::clamp(feetY, c->top, c->bottom) - body.footOffset;
            return result;
        }
    }

    if (input.jump && body.mode == Mode::Grounded) {
        body.vy = -JumpSpeed;
        body.mode = Mode::Rising;
        body.platformId = 0;
    }
    body.vx = input.horizontal * std::max(0.0f, moveSpeed);
    const Vec2 before = pos;
    const float nextX = std::clamp(pos.xPos + body.vx * dt, map.minX, map.maxX);

    if (body.mode == Mode::Grounded) {
        const auto* p = FindPlatform(map, body.platformId);
        if (p && Covers(*p, nextX)) {
            pos.xPos = nextX;
            pos.yPos = p->y - body.footOffset;
            body.vy = 0;
            return result;
        }
        body.mode = Mode::Falling;
        body.platformId = 0;
    }

    body.vy = std::min(body.vy + Gravity * dt, MaxFall);
    const float nextY = before.yPos + body.vy * dt;
    const float oldFeet = before.yPos + body.footOffset;
    const float newFeet = nextY + body.footOffset;
    const Platform* hit = nullptr;
    float bestT = 2.0f;

    // 이전 발 높이→다음 발 높이의 선분으로 검사: 빠르게 떨어져도 관통하지 않는다.
    if (body.vy >= 0 && newFeet > oldFeet) {
        for (const auto& p : map.platforms) {
            if (oldFeet > p.y + Epsilon || newFeet < p.y) continue;
            const float t = std::clamp((p.y - oldFeet) / (newFeet - oldFeet), 0.0f, 1.0f);
            const float hitX = before.xPos + (nextX - before.xPos) * t;
            if (Covers(p, hitX) && t < bestT) { hit = &p; bestT = t; }
        }
    }
    pos = {nextX, nextY};
    if (hit) {
        result.landed = true;
        body.vy = 0;
        // 착지 후 같은 스텝에 가장자리를 지나면 다음 스텝부터 낙하.
        pos.yPos = hit->y - body.footOffset;
        body.mode = Covers(*hit, nextX) ? Mode::Grounded : Mode::Falling;
        body.platformId = body.mode == Mode::Grounded ? hit->id : 0;
    } else {
        body.mode = body.vy < 0 ? Mode::Rising : Mode::Falling;
    }
    result.fellOut = pos.yPos + body.footOffset > map.killY;
    return result;
}
} // namespace Movement
```

**이해할 부분:** 속도는 `vy += gravity * dt`, 위치는 `y += vy * dt`다. 발판 충돌은 최종 좌표의 겹침만 검사하지 않고 지나온 구간을 검사한다. 발의 X 중앙을 착지 기준으로 사용하므로 캐릭터 반쪽이 걸친 착지는 이번 정책에 없다.

## 3. 맵 좌표 입력과 검증

각 `CHANNEL/data/Maps/*.json`의 최상위에 `physics` 키를 추가한다. 아래는 **예제 좌표**이며 실제 배경 이미지/포탈/스폰 좌표에 맞춰 바꾼다. `spawnBounds`는 기존 몬스터 생성용 범위이므로 물리 월드 경계로 재사용하지 않는다.

```json
"physics": {
  "minX": 0,
  "maxX": 1600,
  "killY": 1200,
  "safeFeet": { "x": 100, "y": 700 },
  "platforms": [
    { "id": 1, "left": 0, "right": 1600, "y": 700 },
    { "id": 2, "left": 300, "right": 650, "y": 500 },
    { "id": 3, "left": 850, "right": 1200, "y": 350 }
  ],
  "climbables": [
    { "id": 1, "kind": "ladder", "x": 450, "top": 500,
      "bottom": 700, "grabRange": 16, "topPlatformId": 2 },
    { "id": 2, "kind": "rope", "x": 1000, "top": 350,
      "bottom": 700, "grabRange": 16, "topPlatformId": 3 }
  ]
}
```

**신규: `CHANNEL/util/MovementMapLoader.h` — 전체 코드**

```cpp
#pragma once
#include "MovementPhysics.h"
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <string>

namespace Movement {
inline Map LoadMap(const nlohmann::json& root) {
    const auto& j = root.at("physics");
    Map map;
    auto number = [](const nlohmann::json& obj, const char* key) {
        const float value = obj.at(key).get<float>();
        if (!std::isfinite(value)) throw std::runtime_error("non-finite physics value");
        return value;
    };
    map.minX = number(j, "minX");
    map.maxX = number(j, "maxX");
    map.killY = number(j, "killY");
    map.safeFeet = {number(j.at("safeFeet"), "x"), number(j.at("safeFeet"), "y")};
    if (map.minX >= map.maxX) throw std::runtime_error("invalid world bounds");
    std::set<int> ids;
    for (const auto& v : j.at("platforms")) {
        Platform p{v.at("id").get<int>(), number(v,"left"), number(v,"right"), number(v,"y")};
        if (p.id <= 0 || !ids.insert(p.id).second || p.left >= p.right ||
            p.left < map.minX || p.right > map.maxX || p.y >= map.killY)
            throw std::runtime_error("invalid platform");
        map.platforms.push_back(p);
    }
    if (!Support(map, map.safeFeet.xPos, map.safeFeet.yPos))
        throw std::runtime_error("safeFeet must be on a platform");
    ids.clear();
    for (const auto& v : j.at("climbables")) {
        Climbable c;
        c.id = v.at("id").get<int>();
        const std::string kind = v.at("kind").get<std::string>();
        if (kind != "rope" && kind != "ladder") throw std::runtime_error("invalid climb kind");
        c.kind = kind == "rope" ? ClimbKind::Rope : ClimbKind::Ladder;
        c.x = number(v,"x"); c.top = number(v,"top"); c.bottom = number(v,"bottom");
        c.grabRange = number(v,"grabRange");
        c.topPlatformId = v.at("topPlatformId").get<int>();
        const auto* p = FindPlatform(map, c.topPlatformId);
        if (c.id <= 0 || !ids.insert(c.id).second || c.top >= c.bottom ||
            c.grabRange <= 0 || c.x < map.minX || c.x > map.maxX || c.bottom >= map.killY ||
            !p || !Covers(*p,c.x) || std::fabs(p->y-c.top) > Epsilon)
            throw std::runtime_error("invalid climbable or top platform");
        map.climbables.push_back(c);
    }
    return map;
}
} // namespace Movement
```

추가 위치:

```cpp
// CommonEnum.h: include 추가
#include "MovementPhysics.h"
// MapInitData 멤버 추가
Movement::Map physics;

// MapManager.cpp: include 추가
#include "MovementMapLoader.h"
// LoadJsonFile()의 try 안, LoadPortal() 호출 다음
mapData.physics = Movement::LoadMap(j);

// MapInstance.h: private 멤버
Movement::Map m_physics;
// public 접근자: 로딩 이후에는 읽기 전용
const Movement::Map& GetPhysicsMap() const { return m_physics; }

// MapInstance::Init(): InitSpawnMonster() 전에
m_physics = data.physics;
```

`MapData.h`↔`CommonEnum.h`에 이미 상호 include가 있다. 새 물리 타입은 그 두 헤더를 include하지 않는 독립 헤더로 두어 순환 의존성을 늘리지 않는다.

**확인:** 모든 맵에 physics를 넣고 서버 시작. 발판 ID 중복·사다리 상단 불일치·안전 위치 오류 시 기존 `LoadJsonFile()`의 catch로 시작 실패 이유가 기록되어야 한다.

## 4. 먼저 게임 상태 갱신을 직렬화

현재 `MapManager::Update()`는 같은 맵을 스레드풀에 반복 제출한다. 물리 적용 전, 맵 갱신·입력·퇴장·삭제가 겹치지 않게 해야 한다. 단순히 물리 함수에 mutex 하나만 넣으면 `MapUpdateTask`가 삭제된 맵 포인터를 사용하는 문제는 남는다.

학습용 첫 구현은 **채널 게임 상태용 공통 잠금 + 맵 직접 갱신**으로 맞춘다. DB 작업까지 포함되어 처리량이 낮아질 수 있다. 향후 맵별 명령 큐로 분리할 때 대체할 수 있다.

**신규: `CHANNEL/util/GameplayGate.h`**

```cpp
#pragma once
#include <mutex>
namespace Gameplay {
inline std::recursive_mutex gate;
using Guard = std::lock_guard<std::recursive_mutex>;
}
```

아래 파일에 include하고, 지정 범위의 **첫 잠금**으로 `Gameplay::Guard guard(Gameplay::gate);`를 둔다. 공통 잠금 이후에 기존 player/stat/map mutex를 잡는다.

| 파일/함수 | 잠금 범위 |
|---|---|
| `PacketProcessTask::Execute()` | `try` 내부 맨 앞~핸들러 종료. `BeginValidSessionTask()` 다음이며 `EndSessionTask()` 전에 해제 |
| `PlayerHandler::Execute()` | 직접 테스트 호출도 보호하도록 함수 맨 앞 |
| `MapService::EnterMap()`, `MoveByPortal()` | 함수 전체 |
| `MapInstance::OnEnter()`, `OnLeave()`, `Update()` | 함수 전체 |
| `MapManager::GetOrCreate()`, `RemoveMap()` | 기존 map mutex 이전~함수 종료 |
| `ChannelSession::~ChannelSession()` | `m_player` 접근 이전~퇴장/플레이어 제거 완료 |

`ChannelSession.cpp`의 session destructor 진입 전에 다른 스레드가 gameplay gate를 기다리는 상태로 session 잠금을 보유하지 않는지도 확인한다. **잠금 상태에서 `Stop()/join()`이나 다른 작업 완료를 기다리면 안 된다.**

`MapManager::Update()`의 기존 `for(m_maps...)`와 바로 뒤 `RemoveMap()` 부분을 다음으로 교체한다. 기존 시간 계산과 `wait_for()`는 바깥에 유지한다.

```cpp
{
    Gameplay::Guard guard(Gameplay::gate);
    // GetOrCreate/RemoveMap도 같은 gate를 잡으므로 순회 중 변경되지 않는다.
    for (auto& entry : m_maps) {
        if (!m_running.load(std::memory_order_acquire)) break;
        if (entry.second) entry.second->Update(deltaTime);
    }
    RemoveMap();
}
// 기존 wait_for는 이 아래: gate를 잡은 채 기다리지 않는다.
```

이제 `MapUpdateTask`는 제출하지 않는다. 전체 종료 시에는 `MapManager::Stop()`으로 갱신 스레드를 종료한 뒤 객체를 제거한다.

`RemoveMap()`에서 삭제 직전에 빈 맵인지 재검사한다. 삭제 예약 후 재입장한 경우를 막는다.

```cpp
// 기존 delete it->second 바로 위
if (it->second && it->second->HasPlayer()) continue;
```

뒤 코드에서 `MovementBody()` 등 참조 접근은 **이 gate 안에서만** 한다. 스냅샷 DB 저장처럼 기존 stat/position mutex를 사용하는 별도 읽기 경로는 유지한다. 새로운 직접 호출을 추가하면 이 진입 규칙도 지킨다.

## 5. Player에 입력과 물리 상태 추가

**`Player.h`: include와 public/private 멤버 추가**

```cpp
#include "MovementPhysics.h"

// public:
Movement::Body& MovementBody() { return m_movement; }
const Movement::Body& MovementBody() const { return m_movement; }
int MovementEpoch() const { return m_movementEpoch; }
int LastInputSequence() const { return m_inputSequence; }
bool AcceptMovement(int epoch, int sequence, Movement::Input input);
Movement::Input ConsumeMovement(float dt);
void ResetMovement();
void SetFacing(int facing) { if (facing == -1 || facing == 1) m_dir = facing; }

// private:
Movement::Body m_movement;
Movement::Input m_moveInput;
int m_movementEpoch = 0;
int m_inputSequence = 0;
float m_inputAge = 0;
```

**`Player.cpp`: 함수 추가**

```cpp
void Player::ResetMovement() {
    Movement::Reset(m_movement);
    m_moveInput = {};
    m_inputAge = 0;
    m_inputSequence = 0;
    // 한 접속에서 INT_MAX까지 초기화되는 상황은 프로토콜 재접속 대상으로 둔다.
    ++m_movementEpoch;
}

bool Player::AcceptMovement(int epoch, int sequence, Movement::Input input) {
    if (!IsAlive() || GetState() == PlayerState::STUNNED ||
        epoch != m_movementEpoch || sequence <= m_inputSequence ||
        input.horizontal < -1 || input.horizontal > 1 ||
        input.vertical < -1 || input.vertical > 1) return false;
    m_inputSequence = sequence;
    m_moveInput.horizontal = input.horizontal;
    m_moveInput.vertical = input.vertical;
    // 한 물리 스텝 전 down/up 패킷이 둘 다 도착해도 점프 1회는 보존.
    m_moveInput.jump = m_moveInput.jump || input.jump;
    m_inputAge = 0;
    return true;
}

Movement::Input Player::ConsumeMovement(float dt) {
    m_inputAge += dt;
    if (m_inputAge > 0.3f) m_moveInput = {}; // 연결 지연 때 계속 걷지 않음
    Movement::Input result = m_moveInput;
    m_moveInput.jump = false;
    return result;
}
```

footOffset은 캐릭터 그림의 원점이 발이면 0이다. 기존 충돌체 바닥과 발을 맞춘다면 다음 계산을 초기 설정 후 한 번 실행한다. 그림 기준이 다르면 전용 숫자로 설정한다.

```cpp
// Player::SetInitData의 최종 초기화 지점. Monster::Init도 같은 계산 사용.
const Collider2D collider = GetCollider();
m_movement.footOffset = collider.type == ColliderType::Rect2D
    ? collider.rect.offset.yPos + collider.rect.halfH
    : collider.type == ColliderType::Circle2D
        ? collider.circle.offset.yPos + collider.circle.radius : 0.0f;
```

이동 시마다 `SetPos()`가 save dirty를 표시하는 기존 동작은 유지한다. 다만 위치가 그대로면 호출하지 않는다.

## 6. 입력 패킷과 서버 스냅샷

### 6-1. 프로토콜

기존 `PKT_PLAYER_MOVE`의 의미를 조용히 바꾸지 말고 새 opcode를 추가한다. 아래 값은 확인한 현재 enum에서 빈 값이며, 병합 시 팀의 추가 opcode와 충돌하지 않는지 다시 확인한다.

```cpp
// Packet.h enum: 플레이어 구간에 추가
PKT_MOVEMENT_INPUT    = 0x002C,
PKT_MOVEMENT_SNAPSHOT = 0x002D,
```

| 패킷 | 필드 순서 |
|---|---|
| INPUT C→S | `mapId, epoch, sequence, horizontal, vertical, jumpPressed` |
| SNAPSHOT S→C | `mapId, kind, id, epoch, tick, lastInputSeq, x, y, vx, vy, moveMode, facing, climbId, lifeState, hp, maxHp` |

- 모든 필드는 기존 문자열 필드 포맷. `kind`: 0=플레이어, 1=몬스터.
- `epoch`: 부활/이동/추락 때 증가. 이전 위치에 대한 입력과 보간 기록을 버리는 기준.
- `tick`: 맵의 물리 스텝 번호. 같은 epoch에서 이전 tick은 무시한다.
- `lifeState`: 플레이어는 PlayerState, 몬스터는 MonsterState. 두 enum을 혼용하지 않는다.
- 스냅샷은 `Send()`이므로 `ok`가 앞에 붙지 않는다. 입력 성공 ACK는 생략하고 스냅샷을 사용한다.
- 패킷 하나에 객체 하나를 넣는 첫 구현이다. 추후 배치할 때 최대 패킷 길이와 분할을 처리한다.

### 6-2. 입력 핸들러

`PlayerHandler.h`에 `void MovementInputPacket(PacketContext* ctx);`를 선언하고, **기존 `MovePacket.cpp`에 아래 함수 추가**.

```cpp
void PlayerHandler::MovementInputPacket(PacketContext* ctx) {
    if (!ctx || !ctx->channel_session) return;
    auto* session = ctx->channel_session;
    auto* player = session->GetPlayer();
    if (!player || !player->GetCurrentMap()) return;
    size_t offset = 0;
    std::string error;
    int mapId = 0, epoch = 0, sequence = 0, horizontal = 0, vertical = 0, jump = 0;
    auto read = [&](int& out) {
        return PacketParser::ParseNextIntField(
            ctx->payload, ctx->payload_len, offset, out, error);
    };
    if (!read(mapId) || !read(epoch) || !read(sequence) || !read(horizontal) ||
        !read(vertical) || !read(jump) ||
        offset != static_cast<size_t>(ctx->payload_len) ||
        sequence <= 0 || (jump != 0 && jump != 1)) {
        session->SendNok(PKT_MOVEMENT_INPUT, "invalid movement input");
        return;
    }
    if (mapId != player->GetMapId()) return;
    player->AcceptMovement(epoch, sequence, {horizontal, vertical, jump == 1});
}
```

두 switch에 각각 추가한다.

```cpp
// ChannelPacketFactory::Create(): PlayerHandler를 생성하는 case 묶음
case PKT_MOVEMENT_INPUT:

// PlayerHandler::Execute(): switch 내부
case PKT_MOVEMENT_INPUT:
    MovementInputPacket(ctx);
    break;
```

새 방식 전환 후 기존 `MovePacket()` **본문 전체를 교체**한다. 좌표 전송 경로가 살아 있으면 중력을 우회한다.

```cpp
void PlayerHandler::MovePacket(PacketContext* ctx) {
    if (ctx && ctx->channel_session)
        ctx->channel_session->SendNok(PKT_PLAYER_MOVE, "use movement input protocol");
}
```

`TryApplyMove()`/`HandleMove()`는 새 플레이 흐름에서 호출하지 않는다. 기존 이동 테스트와 Python 부하 클라이언트도 새 입력 규격으로 전환해야 한다.

## 7. 맵에서 플레이어 물리 실행

**`MapInstance.h` 선언 추가**

```cpp
// public:
void ResetPlayerMovement(Player* player, bool useSafePosition);
void BroadcastMovement();
// private:
void UpdatePlayerPhysics(float dt);
void SimulateStep(float dt);
double m_physicsAccumulator = 0;
std::uint64_t m_physicsTick = 0;
```

**`MapInstance.cpp` 함수 추가**

```cpp
void MapInstance::ResetPlayerMovement(Player* player, bool useSafePosition) {
    if (!player) return;
    player->ResetMovement();
    Vec2 pos = player->GetPos();
    auto& body = player->MovementBody();
    if (useSafePosition) Movement::PlaceOnFeet(pos, body, m_physics, m_physics.safeFeet);
    else Movement::ResetAtOrigin(pos, body, m_physics);
    player->SetPos(pos);
}

void MapInstance::UpdatePlayerPhysics(float dt) {
    // 호출자는 gameplay gate를 보유. map 목록의 기존 잠금도 유지.
    std::lock_guard<std::mutex> lock(m_playerMutex);
    for (const auto& entry : m_playerList) {
        Player* player = entry.second;
        if (!player || !player->IsAlive()) continue;
        auto input = player->ConsumeMovement(dt);
        if (player->GetState() == PlayerState::STUNNED) input = {};
        Vec2 pos = player->GetPos();
        const Vec2 before = pos;
        auto& body = player->MovementBody();
        const auto result = Movement::Step(pos, body, m_physics, input, player->GetMoveSpeed(), dt);
        player->SetFacing(body.facing);
        if (result.fellOut) {
            ResetPlayerMovement(player, true);
        } else if (pos.xPos != before.xPos || pos.yPos != before.yPos) {
            player->SetPos(pos);
        }
    }
}

void MapInstance::SimulateStep(float dt) {
    ++m_physicsTick;
    UpdatePlayerPhysics(dt);
    UpdateMonster(dt); // 8절에서 내부 물리 연결
    m_projectileManager.Update(dt);
    // 충돌 판정은 물리 이동 이후에 한다.
    ProcessRangedDamage(NowMs());
    ProcessContactDamage(NowMs());
}
```

**`MapInstance::Update()` 교체** — 기존 `SpawnMonster()`는 바깥 갱신마다 1회, 물리는 고정 간격으로 실행한다.

```cpp
int MapInstance::Update(float deltaTime) {
    Gameplay::Guard guard(Gameplay::gate);
    if (!HasPlayer()) {
        m_physicsAccumulator = 0;
        RemoveMap(); // MapInstance의 빈 맵 삭제 예약 함수
        return 1;
    }
    if (!std::isfinite(deltaTime) || deltaTime < 0) return 1;
    SpawnMonster();
    m_physicsAccumulator += std::min(deltaTime, 0.25f);
    int steps = 0;
    while (m_physicsAccumulator >= Movement::StepSeconds && steps < 15) {
        SimulateStep(Movement::StepSeconds);
        m_physicsAccumulator -= Movement::StepSeconds;
        ++steps;
    }
    // 서버가 오래 멈추면 밀린 시간을 무한 재생하지 않고 버린다.
    if (steps == 15) m_physicsAccumulator = 0;
    SendMapInfo(); // 기존 투사체/몬스터 패킷 유지 가능
    BroadcastMovement(); // 약 20Hz, 멈춘 객체도 보냄
    return 1;
}
```

60Hz 물리지만 기존 전송 주기는 약 20Hz다. 서버 정지 시간 초과분을 버리므로 과부하에서는 게임 시간이 느려질 수 있다. 이 정책을 숨기지 말고 서버 부하 테스트 때 확인한다.

### 7-1. 물리 스냅샷 전송

아래 함수는 플레이어뿐 아니라 몬스터의 이동 상태도 보낸다. 8절의 몬스터 접근자를 추가한 뒤 빌드한다.

```cpp
void MapInstance::BroadcastMovement() {
    struct Row { std::vector<std::string> fields; };
    std::vector<Row> rows;
    auto add = [&](int kind, int id, int epoch, int seq, Vec2 pos,
                   const Movement::Body& b, int lifeState, int hp, int maxHp) {
        rows.push_back({{
            std::to_string(m_mapID), std::to_string(kind), std::to_string(id),
            std::to_string(epoch), std::to_string(m_physicsTick), std::to_string(seq),
            std::to_string(pos.xPos), std::to_string(pos.yPos),
            std::to_string(b.vx), std::to_string(b.vy),
            std::to_string(static_cast<int>(b.mode)), std::to_string(b.facing),
            std::to_string(b.climbableId), std::to_string(lifeState),
            std::to_string(hp), std::to_string(maxHp)
        }});
    };
    std::vector<Player*> recipients;
    {
        std::lock_guard<std::mutex> lock(m_playerMutex);
        for (const auto& entry : m_playerList) {
            auto* p = entry.second;
            if (!p) continue;
            recipients.push_back(p);
            add(0, p->GetId(), p->MovementEpoch(), p->LastInputSequence(), p->GetPos(),
                p->MovementBody(), static_cast<int>(p->GetState()), p->GetCurHP(), p->GetMaxHP());
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_monsterMutex);
        for (const auto& m : m_monsterList) {
            add(1, m.GetInstanceId(), m.MovementEpoch(), 0, m.GetPos(), m.MovementBody(),
                static_cast<int>(m.GetState()), m.GetCurrentHP(), m.GetMaxHP());
        }
    }
    // gameplay gate가 퇴장/destructor를 막는 동안만 raw pointer 사용.
    for (auto* p : recipients) {
        if (auto* session = p->GetSession())
            for (const auto& row : rows) session->Send(PKT_MOVEMENT_SNAPSHOT, row.fields);
    }
}
```

일반 물리 갱신 외에도 `SendEnterPackets()`의 기존 객체 생성 패킷 전송 **후** `BroadcastMovement()`를 호출한다. 클라이언트는 생성 패킷으로 외형을 만들고 새 스냅샷으로 물리 상태를 설정한다. 기존 `MONSTER_MOVE`는 전환 후 위치를 다시 덮어쓰지 않도록 클라이언트에서 위치 적용을 끈다. 투사체 패킷은 그대로 유지한다.

## 8. 몬스터에도 같은 물리 적용

몬스터 AI는 **어느 방향으로 갈지**, 공통 물리는 **실제로 어디까지 가는지**만 결정한다.

**`Monster.h` 추가**

```cpp
#include "MovementPhysics.h"
// public:
const Movement::Body& MovementBody() const { return m_movement; }
int MovementEpoch() const { return m_movementEpoch; }
// private:
Movement::Body m_movement;
int m_movementEpoch = 1;
int m_moveAxis = 0;
bool m_avoidCliff = true;
bool m_canJump = false;
```

`m_avoidCliff`/`m_canJump`는 몬스터별 규칙이다. 데이터로 제어하려면 아래 멤버와 로딩을 추가한다.

```cpp
// CommonEnum.h: MonsterTemplate 내부
bool avoidCliff = true;
bool canJump = false;

// MonsterManager::LoadJsonFile(): moveSpeed 로딩 다음에 추가
monsterTemplate.avoidCliff = j.value("avoidCliff", true);
monsterTemplate.canJump = j.value("canJump", false);

// Monster::Init()
m_avoidCliff = monsterTemplate.avoidCliff;
m_canJump = monsterTemplate.canJump;
```

`Monster::Init()`의 collider 설정이 끝난 뒤 5절의 footOffset 계산을 추가한다. 이어서:

```cpp
Movement::ResetAtOrigin(m_Pos, m_movement, m_mapInstance->GetPhysicsMap());
```

**`UpdatePatrol()` 교체:**

```cpp
int Monster::UpdatePatrol(float dt) {
    (void)dt;
    if (m_Pos.xPos >= m_rightBound) m_dir.xPos = -1;
    if (m_Pos.xPos <= m_leftBound) m_dir.xPos = 1;
    m_moveAxis = m_dir.xPos < 0 ? -1 : 1;
    return 0;
}
```

기존 순찰 범위 `스폰 X ±3`은 매우 좁다. 예제로 `±100`으로 바꾸고 실제 맵/몹 크기에 맞춘다. 발판 끝은 아래 규칙이 별도로 막는다.

**`UpdateChase()` 교체:** X축 이동 의도와 투사체 조준 방향을 분리한다.

```cpp
int Monster::UpdateChase(float dt) {
    Player* player = m_lastAttacker;
    if (!player || !player->IsAlive() || !player->GetCurrentMap() ||
        player->GetCurrentMap()->GetMapId() != m_mapId) {
        m_lastAttacker = nullptr;
        m_lastAttackerId = 0;
        m_state = E_Patrol;
        return UpdatePatrol(dt);
    }
    const Vec2 target = player->GetPos();
    const float dx = target.xPos - m_Pos.xPos;
    const float dy = target.yPos - m_Pos.yPos;
    if (std::fabs(dx) > 0.01f) m_dir.xPos = dx < 0 ? -1.0f : 1.0f;
    m_dir.yPos = 0; // 걸음 방향에는 수직 추적을 사용하지 않는다.
    const float distance = std::sqrt(dx*dx + dy*dy);
    if (m_isRangedAttack && distance <= m_ragedAttackRange) {
        const Vec2 aim = distance > 0.01f
            ? Vec2{dx/distance, dy/distance} : Vec2{m_dir.xPos, 0};
        TryRangedAttack(aim);
        return 0;
    }
    if (std::fabs(dx) < 5.0f) return 0;
    m_moveAxis = dx < 0 ? -1 : 1;
    return 0;
}
```

기존 `TryRangedAttack()`의 `K_LOG_TRACE("... %f", dir)`는 Vec2를 `%f`로 출력한다. 이 부분도 `K_LOG_TRACE("Projectile dir: (%f, %f)", dir.xPos, dir.yPos);`로 교체한다.

**`Monster::Update()` 교체:**

```cpp
int Monster::Update(float dt) {
    if (!m_isAlive) return 0;
    m_moveAxis = 0;
    switch (m_state) {
    case E_Idle: case E_Move: case E_Patrol: UpdatePatrol(dt); break;
    case E_Chase: case E_RangeAttack: UpdateChase(dt); break;
    default: break;
    }
    const auto& map = m_mapInstance->GetPhysicsMap();
    Movement::Input input{m_moveAxis, 0, false};
    const float nextX = m_Pos.xPos + input.horizontal * m_moveSpeed * dt;
    const auto* floor = Movement::FindPlatform(map, m_movement.platformId);
    if (m_movement.mode == Movement::Mode::Grounded && floor && !Movement::Covers(*floor, nextX)) {
        if (m_canJump) {
            input.jump = true; // 첫 정책: 가장자리에서 1회 점프. 길찾기는 별도 기능.
        } else if (m_avoidCliff) {
            input.horizontal = 0;
            m_dir.xPos = -m_dir.xPos;
            if (m_state == E_Chase || m_state == E_RangeAttack) {
                m_lastAttacker = nullptr;
                m_lastAttackerId = 0;
                m_state = E_Patrol;
            }
        }
    }
    const auto result = Movement::Step(m_Pos, m_movement, map, input,
                                       static_cast<float>(m_moveSpeed), dt);
    if (result.fellOut) {
        m_Pos = m_spawnPos;
        Movement::ResetAtOrigin(m_Pos, m_movement, map);
        ++m_movementEpoch;
    }
    return 0;
}
```

`Monster::Dead()`의 실제 사망 분기에 `Movement::Reset(m_movement);`를 추가한다. `Monster::Reset()`에서 스폰 좌표를 복구한 다음 아래를 추가한다.

```cpp
Movement::ResetAtOrigin(m_Pos, m_movement, m_mapInstance->GetPhysicsMap());
++m_movementEpoch;
```

스폰 위치는 원점 기준이다. **발판 Y − footOffset**으로 스폰 원점 Y를 설정하거나, 그보다 위에서 떨어지도록 설정한다. 발판 아래에서 스폰시키면 단방향 발판을 붙잡지 못한다.

### 8-1. 추적 대상 수명 정리

`m_lastAttacker`는 raw pointer다. 플레이어 퇴장 후 접근하면 중력과 별개로 잘못된 포인터를 읽는다.

```cpp
// Monster.h: public
void ClearTarget(int playerId) {
    if (m_lastAttackerId != playerId) return;
    m_lastAttacker = nullptr;
    m_lastAttackerId = 0;
    if (m_isAlive) m_state = E_Patrol;
}

// MapInstance::OnLeave(): player 목록 잠금 블록이 끝난 뒤, SendPlayerLeave 전에
{
    std::lock_guard<std::mutex> lock(m_monsterMutex);
    for (auto& monster : m_monsterList) monster.ClearTarget(PlayerID);
}
```

## 9. 사망·부활·입장·포탈 초기화

| 상황 | 필요한 처리 |
|---|---|
| 사망 | 입력/속도/탑승 해제, epoch 증가, 기존 사망 패킷 |
| 부활 | 안전 발판 좌표, HP·상태 복구, 스탯+물리 스냅샷 |
| 최초 입장/포탈 | 입력 초기화, 목적지 원점에서 발판 판정, 새 epoch |
| 맵 밖 추락 | 안전 지점 이동, 입력 초기화, 새 epoch |
| 포커스 상실 | 클라이언트에서 모든 입력 해제 전송 |
| 몬스터 리스폰 | 스폰 좌표와 물리 상태 초기화, 새 epoch |

**사망:** 현재 `OnDamaged()`는 stat mutex를 잡고 `Dead()`를 호출한다. `Dead()`에서 같은 mutex를 다시 잡거나 `IsAlive()`를 호출하면 교착된다. 기존 잠금 안에서 쓰는 함수라는 계약을 유지한다.

```cpp
// Player::OnDamaged(): m_statMutex 잠금 직후 추가
if (m_CurrentState == PlayerState::DEAD || dmg <= 0) return;

// Player::Dead(): 함수 맨 앞 추가 (직접 호출자는 기존 stat mutex를 보유해야 함)
if (m_CurrentState == PlayerState::DEAD) return;
ResetMovement();
// 뒤의 DEAD 설정/경험치 감소/기존 패킷 흐름은 유지
```

**부활:** `Player::Revive()` 교체.

```cpp
bool Player::Revive(const Vec2& position) {
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        if (m_CurrentState != PlayerState::DEAD) return false;
        m_CurrentState = PlayerState::IDLE;
        m_stat.SetCurHp(m_stat.GetMaxHp());
        m_statDirty = true;
    }
    SetPos(position);
    MarkSaveNeeded();
    return true;
}
```

`MapInstance::RevivePlayer()` 교체. gameplay gate 안에서만 호출한다.

```cpp
bool MapInstance::RevivePlayer(Player* player) {
    if (!player || player->GetCurrentMap() != this || player->IsAlive()) return false;
    Vec2 origin{m_physics.safeFeet.xPos,
                m_physics.safeFeet.yPos - player->MovementBody().footOffset};
    if (!player->Revive(origin)) return false;
    ResetPlayerMovement(player, true);
    PlayerPacketSender::SendPlayerStat(player);
    BroadcastMovement(); // 본인+같은 맵의 다른 사용자
    return true;
}
```

`RevivePacket.cpp`의 `HandleRevivePacket()`도 교체하여 실패를 성공으로 응답하지 않게 한다.

```cpp
void PlayerHandler::HandleRevivePacket(PacketContext* ctx) {
    if (!ctx || !ctx->channel_session) return;
    auto* session = ctx->channel_session;
    auto* player = session->GetPlayer();
    auto* map = player ? player->GetCurrentMap() : nullptr;
    if (!map || !map->RevivePlayer(player)) {
        session->SendNok(PKT_PLAYER_REVIVE, "cannot revive");
        return;
    }
    session->SendOk(PKT_PLAYER_REVIVE);
}
```

**입장:** `MapInstance::OnEnter()` 끝의 `player->ResetMoveValidation();`를 아래로 교체한다. 기존 player 목록 잠금 블록 밖에 둔다.

```cpp
const auto pos = player->GetPos();
const bool invalid = !std::isfinite(pos.xPos) || !std::isfinite(pos.yPos) ||
    pos.xPos < m_physics.minX || pos.xPos > m_physics.maxX ||
    pos.yPos + player->MovementBody().footOffset > m_physics.killY;
ResetPlayerMovement(player, invalid);
```

**포탈:** `MapService::MoveByPortal()`에서 player null 확인 뒤에 추가한다.

```cpp
if (!player->IsAlive()) {
    moveResult.error = "dead player cannot use portal";
    return moveResult;
}
```

포탈 목적지 원점 좌표도 목적지 발판에 맞춘다. `OnEnter()`가 실제로 보정할 수 있으므로 함수 마지막의 `moveResult.spawnPosition`은 `player->GetPos()`로 채운다.

**공격 제한:** `Player::CanUseSkill()`의 기존 STUNNED 검사보다 앞에 추가한다. 이 함수가 stat mutex를 이미 보유한 호출자에게서 실행되는지 확인하고, 현재 본문처럼 직접 state를 읽는 방식으로 맞춘다.

```cpp
if (m_CurrentState == PlayerState::DEAD ||
    m_movement.mode == Movement::Mode::Climbing) return false;
```

이번 정책은 공중 공격 허용, 줄타기 중 공격 금지다. 클라이언트도 같은 입력 제한을 적용하지만 최종 검증은 서버가 수행한다.

## 10. 클라이언트 입력과 수신

클라이언트 소스가 제공되지 않아 `Game`, `Network` 같은 기존 클래스명을 추정하지 않는다. 아래 코드는 **신규 C++ 클래스 제안**이다. `sendFields`를 기존 문자열 필드 패킷 송신 함수에 연결하고, 위치/모션을 기존 캐릭터 객체에 전달한다.

### 10-1. 입력 송신 객체

**제안 파일: `MovementInputController.h`**

```cpp
#pragma once
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

class MovementInputController {
public:
    using Sender = std::function<void(const std::vector<std::string>&)>;
    Sender sendFields; // opcode PKT_MOVEMENT_INPUT로 전송하는 함수를 연결

    void SetContext(int mapId, int epoch, bool alive) {
        const bool reset = mapId != map_ || epoch != epoch_ || alive != alive_;
        map_ = mapId; epoch_ = epoch; alive_ = alive;
        ready_ = true;
        if (reset) { seq_ = 0; ClearKeys(); timer_ = 0; }
    }
    void Horizontal(bool left, bool right) { left_ = left; right_ = right; dirty_ = true; }
    void Vertical(bool up, bool down) { up_ = up; down_ = down; dirty_ = true; }
    void Jump(bool pressed) {
        if (pressed && !jumpHeld_) { jumpPending_ = true; dirty_ = true; }
        jumpHeld_ = pressed;
    }
    void ClearKeys() {
        left_ = right_ = up_ = down_ = jumpHeld_ = jumpPending_ = false;
        dirty_ = true;
    }
    void Update(float dt) {
        if (!ready_ || !alive_ || !sendFields) return;
        timer_ += std::max(0.0f, dt);
        if (!dirty_ && timer_ < 0.1f) return; // 정지 상태도 heartbeat
        sendFields({std::to_string(map_), std::to_string(epoch_), std::to_string(++seq_),
            std::to_string(static_cast<int>(right_) - static_cast<int>(left_)),
            std::to_string(static_cast<int>(down_) - static_cast<int>(up_)),
            jumpPending_ ? "1" : "0"});
        jumpPending_ = false; dirty_ = false; timer_ = 0;
    }
    void OnMapLeave() { ready_ = false; ClearKeys(); }
private:
    int map_ = 0, epoch_ = 0, seq_ = 0;
    bool ready_ = false, alive_ = false, dirty_ = true;
    bool left_ = false, right_ = false, up_ = false, down_ = false;
    bool jumpHeld_ = false, jumpPending_ = false;
    float timer_ = 0;
};
```

연결 순서:

1. 내 캐릭터 최초 물리 스냅샷을 받으면 `SetContext(mapId, epoch, alive)`.
2. WinAPI 키 Down/Up에서 좌우/상하 상태를 갱신하고 Space는 `Jump(true/false)`.
3. 매 프레임 `Update(deltaSeconds)`.
4. 포커스를 잃으면 `ClearKeys()` 후 즉시 `Update(0)`로 해제 전송.
5. 맵을 나가면 `OnMapLeave()`와 모든 원격 보간 버퍼 제거.
6. 죽음 스냅샷에서는 `SetContext(..., false)`로 입력 차단. 공격/아이템/포탈 입력의 기존 게이트에도 생명 상태를 반영.

### 10-2. 스냅샷 파싱

**제안 파일: `MovementSnapshot.h`**. 네트워크 스레드는 수신 객체를 큐에 넣고, 아래 파싱 결과 적용/렌더링은 게임 스레드에서 한다.

```cpp
#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

struct MovementSnapshot {
    int map = 0, kind = 0, id = 0, epoch = 0, sequence = 0;
    std::uint64_t tick = 0;
    float x = 0, y = 0, vx = 0, vy = 0;
    int mode = 0, facing = 1, climbId = 0, lifeState = 0, hp = 0, maxHp = 0;
};
inline bool ParseMovementSnapshot(const std::vector<std::string>& f, MovementSnapshot& out) {
    if (f.size() != 16) return false;
    try {
        auto integer = [&](int index) {
            size_t n = 0; int v = std::stoi(f[index], &n);
            if (n != f[index].size()) throw std::runtime_error("integer field");
            return v;
        };
        auto real = [&](int index) {
            size_t n = 0; float v = std::stof(f[index], &n);
            if (n != f[index].size() || !std::isfinite(v)) throw std::runtime_error("float field");
            return v;
        };
        MovementSnapshot s;
        s.map=integer(0); s.kind=integer(1); s.id=integer(2); s.epoch=integer(3);
        if (f[4].empty() || f[4].find_first_not_of("0123456789") != std::string::npos) return false;
        size_t n=0; s.tick=std::stoull(f[4],&n); if(n!=f[4].size()) return false;
        s.sequence=integer(5); s.x=real(6); s.y=real(7); s.vx=real(8); s.vy=real(9);
        s.mode=integer(10); s.facing=integer(11); s.climbId=integer(12);
        s.lifeState=integer(13); s.hp=integer(14); s.maxHp=integer(15);
        if (s.kind<0 || s.kind>1 || s.mode<0 || s.mode>3 ||
            (s.facing!=-1 && s.facing!=1) || s.epoch<0 || s.hp<0 || s.maxHp<s.hp) return false;
        out=s;
        return true;
    } catch (const std::exception&) { return false; }
}
```

맵 ID가 현재 맵과 다르면 버린다. 객체 키는 `(kind, id)`로 만들고, 아직 외형 생성 패킷이 오지 않은 객체는 최신 스냅샷만 임시 저장한다. 게임 객체 생성 후 적용한다.

## 11. 클라이언트 보간과 애니메이션

첫 단계는 내 캐릭터도 서버 위치를 받아 부드럽게 표시한다. 서버 권위 동작을 먼저 확인할 수 있지만 내 입력에 네트워크 지연이 느껴진다. 즉각 반응을 위한 클라이언트 예측/입력 재실행은 후속 단계로 분리한다. 아래 보간을 예측이라고 부르지 않는다.

**제안 파일: `MovementView.h`**

```cpp
#pragma once
#include "MovementSnapshot.h"
#include <algorithm>
#include <cmath>

struct MovementView {
    MovementSnapshot state;
    bool initialized = false;
    float x = 0, y = 0;
    float startX = 0, startY = 0, targetX = 0, targetY = 0, elapsed = 0;
    float blendSeconds = 0.05f;

    bool Apply(const MovementSnapshot& s) {
        if (initialized && s.map == state.map) {
            if (s.epoch < state.epoch) return false;
            if (s.epoch == state.epoch && s.tick <= state.tick) return false;
        }
        const bool teleport = !initialized || s.map != state.map || s.epoch != state.epoch;
        state = s;
        if (teleport) { x = s.x; y = s.y; }
        startX = x; startY = y;
        targetX = s.x; targetY = s.y;
        elapsed = 0;
        initialized = true;
        return true;
    }
    void Update(float dt) {
        if (!initialized) return;
        elapsed += std::max(0.0f, dt);
        const float t = std::clamp(elapsed / blendSeconds, 0.0f, 1.0f);
        x = startX + (targetX - startX) * t;
        y = startY + (targetY - startY) * t;
    }
};

enum class MovementClip { Idle, Walk, Jump, Fall, Land, Rope, Ladder, Dead };
struct AnimationChoice { MovementClip clip; bool playing; };

class MovementAnimator {
public:
    AnimationChoice Update(const MovementSnapshot& s, bool dead,
                           bool isRope, float dt) {
        if (dead) { landTime_ = 0; previousMode_ = s.mode; return {MovementClip::Dead, true}; }
        const bool teleport = epoch_ != s.epoch;
        if (teleport) { epoch_ = s.epoch; previousMode_ = s.mode; landTime_ = 0; }
        if (previousMode_ == 2 && s.mode == 0) landTime_ = 0.12f;
        previousMode_ = s.mode;
        landTime_ = std::max(0.0f, landTime_ - dt);
        if (s.mode == 3) return {isRope ? MovementClip::Rope : MovementClip::Ladder,
                                std::fabs(s.vy) > 0.01f};
        if (s.mode == 1) return {MovementClip::Jump, true};
        if (s.mode == 2) return {MovementClip::Fall, true};
        if (landTime_ > 0) return {MovementClip::Land, true};
        return {std::fabs(s.vx) > 0.01f ? MovementClip::Walk : MovementClip::Idle, true};
    }
private:
    int previousMode_ = 2, epoch_ = -1;
    float landTime_ = 0;
};
```

`dead`는 플레이어 `PlayerState::DEAD` 또는 몬스터 `E_Die/E_Dead`로 판단한다. 몬스터 HP=0도 죽음 표시 근거로 사용할 수 있다. 서버 enum 값을 클라이언트 공통 프로토콜 헤더와 맞춘다.

`isRope`는 공유 맵 JSON의 `climbId → kind`로 찾는다. 애니메이션은 아래처럼 연결한다.

```cpp
// 게임 프레임의 연결 예시. 아래 함수명은 기존 렌더러에 맞춰 구현한다.
// view.Update(dt);
// character.SetPosition(view.x, view.y);
// auto choice = animator.Update(view.state, dead, isRope, dt);
// character.SetAnimation(choice.clip); // clip이 바뀔 때만 프레임을 0으로 초기화
// character.SetAnimationPlaying(choice.playing); // 사다리에 멈춰 있으면 프레임 정지
// character.SetFacing(view.state.facing);
```

공격 애니메이션은 기존 전투 연출과 합친다. 우선순위는 **죽음 > 피격/공격(게임 정책) > 줄타기 > 점프/낙하 > 착지 > 걷기/대기**로 정한다. 공중 공격 중에도 위치 계산과 낙하는 계속한다.

착지는 수신 상태가 Falling→Grounded로 바뀔 때 재생한다. 20Hz 스냅샷 사이에 착지와 재점프가 모두 일어나면 착지 연출이 생략될 수 있다. 반드시 재생해야 하는 게임이라면 별도 `landEventSeq`를 서버 Body/스냅샷에 추가한다.

**사망 UI:** 기존 죽음 패킷 또는 dead 스냅샷에서 표시하고, 부활 버튼은 기존 `PKT_PLAYER_REVIVE`를 전송한다. `ok`만으로 위치를 추측하지 말고, 새 epoch의 살아 있는 스냅샷에서 HP/위치/상태를 적용하고 UI를 닫는다. 늦게 도착한 옛 이동 스냅샷은 epoch/tick 비교로 버린다.

**맵 시각화:** 서버와 동일한 physics JSON을 클라이언트 리소스에 포함한다. 디버그 모드에서 발판 `(left,y)→(right,y)`, 사다리 `(x,top)→(x,bottom)`, 객체의 발 점을 선/점으로 그리면 이미지와 판정의 어긋남을 찾기 쉽다. 월드 좌표에서 카메라 좌표를 뺀 뒤 화면에 그린다. 로프/사다리 데이터 자체가 스프라이트를 만들지는 않으므로 맵 렌더링에도 해당 이미지를 배치한다.

## 12. 직접 확인할 최소 물리 테스트

**제안 파일: `Test/cpp/MovementPhysicsTest.cpp`**. 먼저 2절 헤더만 작성한 상태에서 이 테스트를 돌려도 된다.

```cpp
#include "MovementPhysics.h"
#include <cassert>
#include <cmath>

int main() {
    using namespace Movement;
    Map map;
    map.minX = -100; map.maxX = 500; map.killY = 400;
    map.platforms = {{1,-100,100,100}, {2,0,100,0}};
    map.safeFeet = {0,100};
    map.climbables = {{1,ClimbKind::Ladder,20,0,100,16,2}};
    Body b;
    Vec2 pos{0,-50};
    bool landed = false;
    for (int i=0;i<120;++i) landed = Step(pos,b,map,{},200,StepSeconds).landed || landed;
    assert(landed && b.mode == Mode::Grounded && b.platformId == 2);

    Step(pos,b,map,{0,0,true},200,StepSeconds);
    assert(b.mode == Mode::Rising && b.vy < 0);
    const float firstVelocity = b.vy;
    Step(pos,b,map,{0,0,true},200,StepSeconds);
    assert(b.vy > firstVelocity); // 공중 점프 입력으로 속도 재설정 금지

    PlaceOnFeet(pos,b,map,{99,100});
    Step(pos,b,map,{1,0,false},200,StepSeconds);
    assert(b.mode == Mode::Falling);

    pos={0,90}; Reset(b); b.vy=MaxFall;
    Step(pos,b,map,{},200,StepSeconds);
    assert(b.platformId == 1 && std::fabs(pos.yPos-100)<Epsilon);

    PlaceOnFeet(pos,b,map,{20,100});
    Step(pos,b,map,{0,-1,false},200,StepSeconds);
    assert(b.mode == Mode::Climbing);
    const float climbY=pos.yPos;
    Step(pos,b,map,{},200,StepSeconds);
    assert(pos.yPos == climbY && b.vy == 0);
    Step(pos,b,map,{1,0,true},200,StepSeconds);
    assert(b.mode == Mode::Rising && b.climbableId == 0);

    PlaceOnFeet(pos,b,map,{20,100});
    for(int i=0;i<40;++i) Step(pos,b,map,{0,-1,false},200,StepSeconds);
    assert(b.mode == Mode::Grounded && b.platformId == 2);

    pos={300,399}; Reset(b); b.vy=MaxFall;
    assert(Step(pos,b,map,{},200,StepSeconds).fellOut);

    b.footOffset=20;
    PlaceOnFeet(pos,b,map,{0,100});
    assert(pos.yPos==80 && b.mode==Mode::Grounded);
}
```

실행 명령:

```sh
g++ -std=c++17 -Wall -Wextra -Werror \
  -I SERVER/src/CHANNEL/util \
  Test/cpp/MovementPhysicsTest.cpp -o /tmp/movement_physics_test
/tmp/movement_physics_test
make -C SERVER/src/CHANNEL -j2
```

`PlayerMovementValidationTest`는 옛 좌표 기반 이동 테스트다. 새로운 INPUT 처리, epoch와 seq, 스냅샷을 검증하는 테스트로 전환한다. 기존 테스트를 통과시키려고 좌표 패킷을 다시 허용하지 않는다.

## 13. 구현 완료 확인표

아래 순서대로 확인하면 어디에서 문제가 났는지 좁히기 쉽다.

- [ ] 발판 위 스폰은 정지, 공중 스폰은 낙하한다.
- [ ] Y+ 방향, 발 원점, 화면 좌표 변환이 일치한다.
- [ ] 점프 1회 후 공중 입력을 반복해도 다시 뛰지 않는다.
- [ ] 빠른 낙하·가장자리 이탈·위층 아래에서 점프가 정상이다.
- [ ] 로프/사다리를 위·아래 키로 잡고 정지·끝 도달·점프 이탈한다.
- [ ] 몬스터가 추적할 때 공중을 대각선으로 걷지 않는다.
- [ ] 절벽 회피/점프 가능 몬스터 설정이 다르게 동작한다.
- [ ] 기존 좌표 이동 패킷으로 공중 이동을 강제할 수 없다.
- [ ] 입력 순서 역전·중복·옛 epoch·다른 맵·잘못된 축 값이 거부된다.
- [ ] 입력 300ms 중단 및 창 포커스 상실 후 수평 이동이 멈춘다.
- [ ] 사망 중 입력·공격·포탈이 차단되고 경험치 패널티가 한 번만 적용된다.
- [ ] 부활/추락/포탈 후 속도·입력·보간이 초기화되고 다른 화면에도 반영된다.
- [ ] 맵 재입장 시 삭제 예약 때문에 맵이 사라지지 않는다.
- [ ] 몬스터가 쫓던 사용자가 접속 종료해도 대상 포인터를 다시 읽지 않는다.
- [ ] 두 클라이언트에서 좌표·점프·줄타기·사망·HP가 일치한다.
- [ ] 50ms 이상 서버 지연 상황에서 물리 계산이 겹치거나 한없이 밀리지 않는다.

## 14. 추천 작업 단위

| 순서 | 한 번에 끝낼 범위 | 확인 기준 |
|---|---|---|
| 1 | 공통 물리 헤더 + 테스트 | 작은 테스트 프로그램 통과 |
| 2 | 맵 데이터 + JSON 로더 | 잘못된 맵 데이터 시작 거부 |
| 3 | 실행 직렬화 + Player + 맵 물리 | 서버 좌표가 중력에 따라 변함 |
| 4 | INPUT/SNAPSHOT + 클라이언트 | 내 캐릭터 이동·점프·보간 |
| 5 | 로프·사다리 + 애니메이션 | 탑승/정지/이탈/끝 처리 |
| 6 | 몬스터 물리·추적·리스폰 | 같은 발판 규칙 적용 |
| 7 | 죽음·부활·포탈·추락 | 이전 입력/속도 영향 없음 |
| 8 | 두 클라이언트 검증 | 다른 화면에서도 동일한 결과 |

물리 핵심 코드는 그대로 작성하며 학습할 수 있는 예제다. 실제 맵 좌표, 그림의 footOffset, 클라이언트 렌더러/송수신 함수 연결은 프로젝트 자산과 클래스에 맞춰 채운다. 특히 4절의 공통 잠금은 학습용 직렬화 방식이므로, 다수 사용자 성능 최적화 단계에서는 맵별 소유권·명령 큐 설계로 바꾼다.

## 문서 예제 검증 범위

문서 작성 시 물리 헤더와 12절 테스트를 임시 폴더로 추출하여 C++17, `-Wall -Wextra -Werror`로 컴파일하고 실행했다. 클라이언트 입력·스냅샷·보간 헤더도 함께 컴파일했다. 실제 게임 소스에 코드를 적용하지 않았으므로 서버 전체 통합 빌드, WinAPI 렌더러 연결, 멀티클라이언트 실행은 아직 수행하지 않았다. 13절은 직접 구현한 뒤 수행할 통합 확인 목록이다.
