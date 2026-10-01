#pragma once

#include <memory>
#include <string>

class IMonsterBehavior;

class MonsterBehaviorFactory
{
public:
    static std::unique_ptr<IMonsterBehavior> Create(const std::string& behaviorType);
};