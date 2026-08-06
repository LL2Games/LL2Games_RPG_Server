#pragma once
#include "Packet.h"
#include "IPacketHandler.h"

class CheckDupNickHandler : public IPacketHandler
{
public:
    void Execute(PacketContext* ctx) override;
};