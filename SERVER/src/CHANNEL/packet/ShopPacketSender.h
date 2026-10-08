#pragma once

#include "NPCData.h"
#include "ShopData.h"

class ChannelSession;

class ShopPacketSender
{
public:
    static void SendOpen(ChannelSession* session, const NPCInteractionResult& npc, const ShopDefinition& shop);
    static void SendBuySuccess(ChannelSession* session,const ShopBuyResult& result);
    static void SendSellSuccess(ChannelSession* session,const ShopSellResult& result);
};