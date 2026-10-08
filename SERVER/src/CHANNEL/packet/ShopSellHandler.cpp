#include "ShopSellHandler.h"

#include "Packet.h"
#include "PacketParser.h"
#include "ChannelSession.h"
#include "ShopService.h"
#include "ShopPacketSender.h"

#include <string>

void ShopSellHandler::Execute(PacketContext* ctx)
{
    if (ctx == nullptr || ctx->channel_session == nullptr)
        return;

    ChannelSession* session = ctx->channel_session;

    if (!session->IsAuthenticated())
    {
        session->SendNok(PKT_SHOP_BUY, "not authenticated");
        return;
    }

    if (ctx->shop_service == nullptr)
    {
        session->SendNok(PKT_SHOP_BUY, "shop service unavailable");
        return;
    }

    if (ctx->payload == nullptr || ctx->payload_len <= 0)
    {
        session->SendNok(PKT_SHOP_BUY, "empty payload");
        return;
    }

    std::size_t offset = 0;
    const std::size_t payloadSize = static_cast<std::size_t>(ctx->payload_len);

    int mapId = 0;
    int spawnId = 0;
    int inventoryType = 0;
    int slotPos = 0;
    int count = 0;

    std::string errMsg;

    if (!PacketParser::ParseNextIntField(ctx->payload, payloadSize, offset, mapId, errMsg) ||
        !PacketParser::ParseNextIntField(ctx->payload, payloadSize, offset, spawnId, errMsg) ||
        !PacketParser::ParseNextIntField(ctx->payload, payloadSize, offset, inventoryType, errMsg) ||
        !PacketParser::ParseNextIntField(ctx->payload, payloadSize, offset, slotPos, errMsg) ||
        !PacketParser::ParseNextIntField(ctx->payload, payloadSize, offset, count, errMsg))
    {
        session->SendNok(PKT_SHOP_SELL, errMsg);
        return;
    }

    if (offset != payloadSize)
    {
        session->SendNok(PKT_SHOP_SELL, "unexpected payload fields");
        return;
    }

    ShopSellResult result{};

    if (!ctx->shop_service->Sell(session->GetPlayer(), mapId, spawnId, inventoryType, slotPos, count, result, errMsg))
    {
        session->SendNok(PKT_SHOP_SELL, errMsg);
        return;
    }

    ShopPacketSender::SendSellSuccess(session, result);
}