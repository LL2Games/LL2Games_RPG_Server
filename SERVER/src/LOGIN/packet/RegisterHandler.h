#pragma once
#include "IPacketHandler.h"

// Command
class RegisterHandler : public IPacketHandler{
public:
    void Execute(PacketContext* ctx) override;
};