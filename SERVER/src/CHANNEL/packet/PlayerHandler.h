#pragma once
#include "common.h"
#include "Packet.h"
#include "IPacketHandler.h"
#include "Player.h"
#include "MySqlConnectionPool.h"


class PlayerHandler : public IPacketHandler
{
public:
    PlayerHandler(uint16_t type);
    void Execute(PacketContext * ctx) override;

    //stat
    void HandleStatView(PacketContext* ctx);
    void HandleStatUp(PacketContext* ctx);

    void MovePacket(PacketContext* ctx);
    void SkillAttackPacket(PacketContext* ctx);
    void BasicAttackPacket(PacketContext* ctx);
    void OnDamagedPacket(PacketContext* ctx);
    void UseItemPacket(PacketContext* ctx);
    void PickUpItemPacket(PacketContext* ctx);
  
    //trade
    void HandleTradeRequest(PacketContext* ctx);
    void HandleTradeAccept(PacketContext* ctx);
    void HandleTradeReady(PacketContext* ctx);
    void HandleTradeCancel(PacketContext* ctx);
    void HandleTradeAddItem(PacketContext *ctx);


private:
    uint16_t m_type;
};