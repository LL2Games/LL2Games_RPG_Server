#pragma once

#include "IMonsterBehavior.h"
#include <array>
#include <vector>

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

      enum class RootPattern
    {
        Single = 1,
        HorizontalTriple = 2,
    };

    void BeginRootPattern(Monster& monster, const Vec2& targetPos);


    State m_state = State::Idle;
    RootPattern m_nextPattern = RootPattern::Single;

    float m_timer = 0.0f;

    // 한 번 예고한 위치는 공격이 끝날 때까지 유지한다.
    std::array<Vec2, 3> m_attackCenters{};
    int m_attackCount = 0;

    std::vector<MonsterAction> m_actions;
};