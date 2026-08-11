#pragma once
#include "Packet.h"
#include "IPacketHandler.h"

class CharacterDelHandler : public IPacketHandler
{
public:
    void Execute(PacketContext* ctx) override;
};