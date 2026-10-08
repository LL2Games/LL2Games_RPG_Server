#include "NPCPacketSender.h"
#include "ChannelSession.h"
#include "K_slog.h"

void NPCPacketSender::SendNPCSnapshot(Player* player, const std::vector<NPCSpawnData>& npcs)
{
    if (player == nullptr)
        return;

    ChannelSession* session = player->GetSession();

    if (session == nullptr)
        return;

    std::vector<std::string> payload;
    payload.reserve(1 + npcs.size() * 5);

    payload.push_back(std::to_string(npcs.size()));

    for (const NPCSpawnData& npc : npcs)
    {
        payload.push_back(std::to_string(npc.spawnId));
        payload.push_back(std::to_string(npc.npcId));
        payload.push_back(std::to_string(npc.position.xPos));
        payload.push_back(std::to_string(npc.position.yPos));
        payload.push_back(std::to_string(npc.interactionRange));
    }

    session->Send(PKT_NPC_SNAPSHOT, payload);
}

void NPCPacketSender::SendInteractSuccess(ChannelSession* session, const NPCInteractionResult& result)
{
    if (session == nullptr)
        return;

    std::vector<std::string> payload;
    payload.reserve(7 + result.dialogue.size());

    payload.push_back(std::to_string(result.mapId));
    payload.push_back(std::to_string(result.spawnId));
    payload.push_back(std::to_string(result.npcId));
    payload.push_back(result.name);
    payload.push_back(result.role);

    payload.push_back(std::to_string(result.dialogue.size()));

    for (const std::string& line : result.dialogue)
        payload.push_back(line);

    payload.push_back(std::to_string(result.shopId));

    session->SendOk(PKT_NPC_INTERACT, payload);
}