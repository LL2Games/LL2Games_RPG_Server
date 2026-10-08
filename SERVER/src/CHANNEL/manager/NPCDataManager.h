#pragma once

#include "NPCData.h"

#include <string>
#include <unordered_map>

class NPCDataManager
{
public:
    bool Init();

    const NPCDefinition* Find(int npcId) const;

private:
    std::unordered_map<int, NPCDefinition> m_npcs;
};