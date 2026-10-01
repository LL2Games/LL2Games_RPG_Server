#pragma once

#include "common.h"
#include "Math.h"

struct MonsterAction
{
    enum class Type
    {
        AreaAttack,
        PatternStart
    };

    Type type = Type::AreaAttack;
    int attackerInstanceId = 0;
    Vec2 center{};
    float radius = 0.0f;
    int damage = 0;
    int patternId = 0;
    int telegraphMs = 0;
};