#include "NPCInteractionService.h"
#include "Player.h"
#include "MapInstance.h"
#include "NPCDataManager.h"

#include <cmath>

bool NPCInteractionService::ValidateInteraction(Player* player, int spawnId, NPCSpawnData& outNPC, std::string& errMsg) const
{
    outNPC = {};
    errMsg.clear();

    if (player == nullptr)
    {
        errMsg = "player not found";
        return false;
    }

    if (!player->IsAlive())
    {
        errMsg = "player is dead";
        return false;
    }

    MapInstance* currentMap = player->GetCurrentMap();

    if (currentMap == nullptr)
    {
        errMsg = "current map not found";
        return false;
    }

    // 클라이언트가 보낸 NPC ID를 믿지 않고,
    // 플레이어의 현재 맵에서 배치 정보를 찾는다.
    const auto npc = currentMap->FindNPC(spawnId);

    if (!npc.has_value())
    {
        errMsg = "npc not found";
        return false;
    }

    const Vec2 playerPos = player->GetPos();

    if (!std::isfinite(playerPos.xPos) || !std::isfinite(playerPos.yPos))
    {
        errMsg = "invalid player position";
        return false;
    }

    const double dx = static_cast<double>(playerPos.xPos) - npc->position.xPos;
    const double dy = static_cast<double>(playerPos.yPos) - npc->position.yPos;

    const double range = npc->interactionRange;

    if (dx * dx + dy * dy > range * range)
    {
        errMsg = "npc is out of range";
        return false;
    }

    outNPC = *npc;
    return true;
}

bool NPCInteractionService::Interact(Player* player, int spawnId, NPCInteractionResult& outResult, std::string& errMsg) const
{
    outResult = {};
    errMsg.clear();

    if (m_npcManager == nullptr)
    {
        errMsg = "npc manager unavailable";
        return false;
    }

    NPCSpawnData spawn{};

    if (!ValidateInteraction(player, spawnId, spawn, errMsg))
        return false;

    const NPCDefinition* definition = m_npcManager->Find(spawn.npcId);

    if (definition == nullptr)
    {
        errMsg = "npc definition not found";
        return false;
    }

    outResult.mapId = static_cast<int>(player->GetCurrentMap()->GetMapId());
    outResult.spawnId = spawn.spawnId;
    outResult.npcId = spawn.npcId;
    outResult.name = definition->name;
    outResult.role = definition->role;
    outResult.dialogue = definition->dialogue;

    if (definition->shop.has_value())
        outResult.shopId = definition->shop->shopId;

    return true;
}