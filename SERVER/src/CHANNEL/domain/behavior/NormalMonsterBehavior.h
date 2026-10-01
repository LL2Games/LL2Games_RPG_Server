#pragma once

#include "IMonsterBehavior.h"
#include <cstdint>

class NormalMonsterBehavior final: public IMonsterBehavior
{
public:
    void Initialize(Monster& monster) override;
    void Update(Monster& monster,float deltaTime) override;
};