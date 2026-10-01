#pragma once
#include "common.h"
#include "Math.h"
#include "MonsterAction.h"
class Monster;



class IMonsterBehavior
{
public:
    virtual ~IMonsterBehavior() = default;

    virtual void Initialize(Monster& monster) = 0;
    virtual void Update(Monster& monster,float deltaTime) = 0;
    virtual std::vector<MonsterAction> TakeActions()
    {
        return {};
    }
};