#include "CorruptedGuardianBehavior.h"
#include "../Monster.h"
#include "Player.h"
#include "MapInstance.h"

namespace
{
    constexpr float kStartDistance = 180.0f;
    constexpr float kRootRadius = 30.0f;
    constexpr float kRootSpacing = 90.0f;
    constexpr int kRootDamage = 30;

    constexpr int kTelegraphMs = 800;
    constexpr float kRecoverySeconds = 1.2f;
}

void CorruptedGuardianBehavior::Initialize(Monster& monster)
{
    m_state = State::Idle;
    m_nextPattern = RootPattern::Single;
    m_timer = 0.0f;
    m_attackCount = 0;
    m_attackCenters = {};
    m_actions.clear();

    monster.SetState(E_Idle);
}

void CorruptedGuardianBehavior::BeginRootPattern(Monster& monster, const Vec2& targetPos)
{
    const RootPattern pattern = m_nextPattern;

    // 다음 공격에서는 다른 패턴을 선택한다.
    m_nextPattern = pattern == RootPattern::Single ? RootPattern::HorizontalTriple : RootPattern::Single;

    if (pattern == RootPattern::Single)
    {
        m_attackCount = 1;
        m_attackCenters[0] = targetPos;
    }
    else
    {
        m_attackCount = 3;

        // 같은 순간의 플레이어 위치를 기준으로 세 위치를 저장한다.
        m_attackCenters[0] = targetPos;
        m_attackCenters[0].xPos -= kRootSpacing;

        m_attackCenters[1] = targetPos;

        m_attackCenters[2] = targetPos;
        m_attackCenters[2].xPos += kRootSpacing;
    }

    m_timer = static_cast<float>(kTelegraphMs) / 1000.0f;
    m_state = State::Telegraph;
    monster.SetState(E_RangeAttack);

    // 단발은 예고 1개, 가로 공격은 예고 3개를 보낸다.
    for (int i = 0; i < m_attackCount; ++i)
    {
        m_actions.push_back({
            MonsterAction::Type::PatternStart,
            monster.GetInstanceId(),
            m_attackCenters[i],
            kRootRadius,
            0,
            static_cast<int>(pattern),
            kTelegraphMs
        });
    }
}

void CorruptedGuardianBehavior::Update(Monster& monster, float deltaTime)
{
    if (!monster.IsAlive())
    {
        m_attackCount = 0;
        m_actions.clear();
        return;
    }

    Player* target = monster.GetLastAttacker();

    if (!target || !target->IsAlive() ||
        !target->GetCurrentMap() || target->GetCurrentMap()->GetMapId() != monster.GetMapId())
    {
        m_state = State::Idle;
        m_timer = 0.0f;
        m_attackCount = 0;
        m_actions.clear();

        monster.SetState(E_Idle);
        return;
    }

    switch (m_state)
    {
    case State::Idle:
        m_state = State::Chase;
        monster.SetState(E_Chase);
        break;

    case State::Chase:
    {
        const Vec2 bossPos = monster.GetPos();
        const Vec2 targetPos = target->GetPos();

        const float dx = targetPos.xPos - bossPos.xPos;
        const float dy = targetPos.yPos - bossPos.yPos;

        if (dx * dx + dy * dy > kStartDistance * kStartDistance)
        {
            monster.UpdateChase(deltaTime);
            break;
        }

        BeginRootPattern(monster, targetPos);
        break;
    }

    case State::Telegraph:
        m_timer -= deltaTime;

        if (m_timer > 0.0f)
            break;

        // 저장한 위치 전체에서 같은 서버 업데이트에 공격한다.
        for (int i = 0; i < m_attackCount; ++i)
        {
            m_actions.push_back({
                MonsterAction::Type::AreaAttack,
                monster.GetInstanceId(),
                m_attackCenters[i],
                kRootRadius,
                kRootDamage
            });
        }

        m_attackCount = 0;
        m_timer = kRecoverySeconds;
        m_state = State::Recovery;
        break;

    case State::Recovery:
        m_timer -= deltaTime;

        if (m_timer > 0.0f)
            break;

        m_state = State::Chase;
        monster.SetState(E_Chase);
        break;
    }
}

std::vector<MonsterAction> CorruptedGuardianBehavior::TakeActions()
{
    std::vector<MonsterAction> actions;
    actions.swap(m_actions);
    return actions;
}