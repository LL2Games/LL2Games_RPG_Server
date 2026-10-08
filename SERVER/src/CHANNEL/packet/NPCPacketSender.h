#pragma once
#include "common.h"
#include "Player.h"
#include "MapData.h"
#include "NPCData.h"

class ChannelSession;
class NPCPacketSender
{
public:
    static void SendNPCSnapshot(Player* player,const std::vector<NPCSpawnData>& npcs);
    static void SendInteractSuccess(ChannelSession* session, const NPCInteractionResult& result);
};