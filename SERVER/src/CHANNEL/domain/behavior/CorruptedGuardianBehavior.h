#pragma once

#include "IMonsterBehavior.h"
#include <cstdint>

class CorruptedGuardianBehavior final : public IMonsterBehavior
{
public:
    void Initialize(Monster& monster) override;
    void Update(Monster& monster, float deltaTime) override;
    std::vector<MonsterAction> TakeActions() override;
private:
    enum class State
    {
        Idle,       // 대상이 없거나 전투 시작 전
        Chase,      // 플레이어 추적
        Telegraph,  // 공격 예고
        Recovery,   // 공격 후딜레이
    };

    State m_state = State::Idle;
    int64_t m_stateEndTime = 0;
    float m_timer = 0.0f;
    Vec2 m_slamCenter{};
    std::vector<MonsterAction> m_actions;
};