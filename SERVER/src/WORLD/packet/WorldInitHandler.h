#pragma once
#include "Packet.h"
#include "IPacketHandler.h"

class WorldInitHandler : public IPacketHandler
{
public:
    void Execute(PacketContext *ctx) override;
    
};