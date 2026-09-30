#include "MonsterBehaviorFactory.h"
#include "NormalMonsterBehavior.h"
#include "CorruptedGuardianBehavior.h"
#include <stdexcept>

std::unique_ptr<IMonsterBehavior> MonsterBehaviorFactory::Create(const std::string& behaviorType)
{
    if(behaviorType == "CORRUPTED_GUARDIAN")
    {
        return std::make_unique<CorruptedGuardianBehavior>();
    }

    if(behaviorType == "NORMAL")
    {
        return std::make_unique<NormalMonsterBehavior>();
    }

    throw std::runtime_error("unknown monster behavior: " + behaviorType);
}