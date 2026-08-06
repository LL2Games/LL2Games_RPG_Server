#pragma once
#include "Packet.h"
#include "IPacketHandler.h"

class CharacterNewHandler : public IPacketHandler
{
public:
    void Execute(PacketContext* ctx) override;
};