#pragma once

#include "IPacketHandler.h"

class NPCInteractionHandler : public IPacketHandler
{
public:
    void Execute(PacketContext* ctx) override;
};