#include "NPCInteractionHandler.h"

#include "Packet.h"
#include "PacketParser.h"
#include "ChannelSession.h"
#include "Player.h"
#include "MapInstance.h"
#include "NPCInteractionService.h"
#include "NPCPacketSender.h"
#include "NPCData.h"
#include "ShopManager.h"
#include "ShopPacketSender.h"

#include <string>

void NPCInteractionHandler::Execute(PacketContext* ctx)
{
    if (ctx == nullptr || ctx->channel_session == nullptr)
        return;

    ChannelSession* session = ctx->channel_session;

    if (!session->IsAuthenticated())
    {
        session->SendNok(PKT_NPC_INTERACT, "not authenticated");
        return;
    }

    if (ctx->npc_interaction_service == nullptr)
    {
        session->SendNok(PKT_NPC_INTERACT, "npc interaction service unavailable");
        return;
    }

    if (ctx->payload == nullptr || ctx->payload_len <= 0)
    {
        session->SendNok(PKT_NPC_INTERACT, "empty payload");
        return;
    }

    std::size_t offset = 0;
    const std::size_t payloadSize = static_cast<std::size_t>(ctx->payload_len);

    int mapId = 0;
    int spawnId = 0;
    std::string errMsg;

    if (!PacketParser::ParseNextIntField(ctx->payload, payloadSize, offset, mapId, errMsg) ||
        !PacketParser::ParseNextIntField(ctx->payload, payloadSize, offset, spawnId, errMsg))
    {
        session->SendNok(PKT_NPC_INTERACT, errMsg);
        return;
    }

    if (offset != payloadSize)
    {
        session->SendNok(PKT_NPC_INTERACT, "unexpected payload fields");
        return;
    }

    Player* player = session->GetPlayer();

    if (player == nullptr)
    {
        session->SendNok(PKT_NPC_INTERACT, "player not found");
        return;
    }

    MapInstance* currentMap = player->GetCurrentMap();

    if (mapId <= 0 || currentMap == nullptr || currentMap->GetMapId() != static_cast<std::uint32_t>(mapId))
    {
        session->SendNok(PKT_NPC_INTERACT, "map mismatch");
        return;
    }

    NPCInteractionResult result{};

    if (!ctx->npc_interaction_service->Interact(player, spawnId, result, errMsg))
    {
        session->SendNok(PKT_NPC_INTERACT, errMsg);
        return;
    }

    const ShopDefinition* shop = nullptr;

    // 상점이 있는 NPC라면 서버 상품 데이터를 조회한다.
    if (result.shopId > 0)
    {
        if (ctx->shop_manager == nullptr)
        {
            session->SendNok(PKT_NPC_INTERACT, "shop manager unavailable");
            return;
        }

        shop = ctx->shop_manager->Find(result.shopId);

        if (shop == nullptr)
        {
            session->SendNok(PKT_NPC_INTERACT, "shop definition not found");
            return;
        }
    }

    // 필요한 데이터가 모두 확인된 뒤 성공 응답을 보낸다.
    NPCPacketSender::SendInteractSuccess(session, result);

    if (shop != nullptr)
    {
        ShopPacketSender::SendOpen(session, result, *shop);
    }
}