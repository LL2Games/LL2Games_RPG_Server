#include "NormalMonsterBehavior.h"
#include "../Monster.h"

void NormalMonsterBehavior::Initialize(Monster& monster)
{
    monster.SetState(E_Patrol);
}

void NormalMonsterBehavior::Update(Monster& monster, float deltaTime)
{
    switch (monster.GetState())
    {
    case E_Idle:
    case E_Move:
    case E_Patrol:
        monster.UpdatePatrol(deltaTime);
        break;

    case E_Chase:
    case E_RangeAttack:
        monster.UpdateChase(deltaTime);
        break;

    default:
        break;
    }
}