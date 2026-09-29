#pragma once
#include "common.h"
#include "Player.h"

class NPCPacketSender
{
public:
    static void SendNPCSnapshot(
        Player* player,
        const std::vector<NPCSpawnData>& npcs);
};