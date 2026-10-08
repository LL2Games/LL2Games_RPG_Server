#pragma once

#include "MapData.h"
#include "NPCData.h"
#include <string>

class Player;
class NPCDataManager;

class NPCInteractionService
{
public:
    // 성공했을 때 outNPC에 검증된 NPC 배치 정보를 반환한다.
    bool ValidateInteraction(Player* player, int spawnId, NPCSpawnData& outNPC, std::string& errMsg) const;
    bool Interact(Player* player, int spawnId,NPCInteractionResult& outResult, std::string& errMsg) const;
public:
    void SetNPCManager(const NPCDataManager* manager){m_npcManager = manager;}

private:
    const NPCDataManager* m_npcManager = nullptr;
};