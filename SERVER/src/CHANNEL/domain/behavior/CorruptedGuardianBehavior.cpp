#include "CorruptedGuardianBehavior.h"
#include "../Monster.h"
#include "Player.h"
#include "MapInstance.h"

void CorruptedGuardianBehavior::Initialize(Monster& monster)
{
    m_state = State::Idle;
    monster.SetState(E_Idle);
}

void CorruptedGuardianBehavior::Update(Monster& monster, float deltaTime)
{
   Player* target = monster.GetLastAttacker();

    // 현재 서버는 마지막으로 보스를 공격한 플레이어를 추적 대상으로 사용한다.
    if (!target ||!target->IsAlive() || !target->GetCurrentMap() ||
        target->GetCurrentMap()->GetMapId() != monster.GetMapId())
    {
        m_state = State::Idle;
        m_timer = 0.0f;
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

        if (dx * dx + dy * dy > 180.0f * 180.0f)
        {
            monster.UpdateChase(deltaTime);
            break;
        }

        // 공격 범위에 들어오면 이동을 멈추고 0.8초 예고한다.
        m_slamCenter = targetPos;
        m_timer = 0.8f;
        m_state = State::Telegraph;
        monster.SetState(E_RangeAttack);

        m_actions.push_back({
            MonsterAction::Type::PatternStart,
            monster.GetInstanceId(),
            m_slamCenter,
            30.0f,
            0,
            1,      // patternId: 내려찍기
            800
        });
        break;
    }

    case State::Telegraph:
        m_timer -= deltaTime;
        if (m_timer > 0.0f)
            break;

        m_actions.push_back({MonsterAction::Type::AreaAttack, monster.GetInstanceId(), m_slamCenter, 30.0f, 30});
        m_timer = 1.2f;
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