#pragma once

#include "IPacketHandler.h"

class ShopSellHandler : public IPacketHandler
{
public:
    void Execute(PacketContext* ctx) override;
};