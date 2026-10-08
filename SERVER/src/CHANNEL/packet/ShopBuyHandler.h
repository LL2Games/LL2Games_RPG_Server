#pragma once

#include "IPacketHandler.h"

class ShopBuyHandler : public IPacketHandler
{
public:
    void Execute(PacketContext* ctx) override;
};