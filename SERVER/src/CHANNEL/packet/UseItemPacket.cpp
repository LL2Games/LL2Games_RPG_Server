#include "PlayerHandler.h"
#include "common.h"
#include "PacketParser.h"
#include "ChannelSession.h"
#include "PlayerManager.h"
#include "MapInstance.h"
#include "ItemService.h"
#include "PacketDTO.h"
#include "Inventory_Info.h"
#include "utility.h"
#include "ItemPacketSender.h"



bool TransferData(Str_UseItem& str_itemData, UseItem& itemData);


void PlayerHandler:: UseItemPacket(PacketContext * ctx)
{
    ChannelSession *session = nullptr;
    ItemService* item_service = nullptr;
    size_t offset = 0;
    UseItemResult result;

    Str_UseItem str_itemData{};
    UseItem itemData{};
    int rc = EXIT_SUCCESS;

    std::string str_inventoryType;
    std::string str_slotPos;
    std::string str_itemId;
    std::string str_useCount;
    std::string errMsg;

    std::vector<std::string> useItem_Info;
     
    if(ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }

    session = ctx->channel_session;
    if(session == nullptr)
    {
     K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    item_service = ctx->item_service;
    if(item_service == nullptr)
    {
     K_LOG_ERROR( "item_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "] item_service is nullptr";
        goto err;
    }

    if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        str_inventoryType,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        str_slotPos,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }


    // item_id를 추출
     if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        str_itemId,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

     // use_count 추출
     if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        str_useCount,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    str_itemData.str_inventoryType = str_inventoryType;
    str_itemData.str_slotPos = str_slotPos;
    str_itemData.str_itemId = str_itemId;
    str_itemData.str_useCount =str_useCount;

    if(!TransferData(str_itemData, itemData))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "Transfer fail");
        goto err;
    }

    // ITEM 사용 가능 여부 확인 후 사용
    //Player* player, int itemId, int useCount

    K_LOG_DEBUG( "itemData UseCount [%d]", itemData.useCount);
    rc = item_service->HandleUseItem(session->GetPlayer(), itemData, result);
    if(rc == EXIT_FAILURE)
    {
        K_LOG_ERROR( "HandleUseItem Failed");
        goto err;
    }
    useItem_Info.push_back(std::to_string(result.result));
    useItem_Info.push_back(std::to_string(result.errcode));
    useItem_Info.push_back(std::to_string(result.inventoryType));
    useItem_Info.push_back(std::to_string(result.slotPos));
    useItem_Info.push_back(std::to_string(result.item_id));
    useItem_Info.push_back(std::to_string(result.used_count));
    useItem_Info.push_back(std::to_string(result.remain_count));
    useItem_Info.push_back(std::to_string(result.hp));
    useItem_Info.push_back(std::to_string(result.mp));
    

err:
    if (rc != EXIT_SUCCESS) {
        session->SendNok(PKT_PLAYER_USE_ITEM, errMsg);
    } else {
        K_LOG_TRACE( "UseItemPacket END");
        session->Send(PKT_PLAYER_USE_ITEM, useItem_Info);
    }


}

void PlayerHandler::PickUpItemPacket(PacketContext *ctx)
{
    ChannelSession *session = nullptr;
    Player* player = nullptr;
    MapInstance* mapInstance = nullptr;
    size_t offset = 0;
    int dropItemId = 0;
    std::string errMsg;
    std::vector<AddItemResult> addItemResults;
    int rc = EXIT_SUCCESS;
    K_LOG_ERROR( "PickUpItemPacket Start \n");
    if(ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }

    session = ctx->channel_session;
    if(session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    player = session->GetPlayer();
    if(player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "] player is nullptr";
        goto err;
    }

    mapInstance = player->GetCurrentMap();
    if(mapInstance == nullptr)
    {
        K_LOG_ERROR( "mapInstance is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "] mapInstance is nullptr";
        goto err;
    }

    if(!PacketParser::ParseNextIntField(ctx->payload, ctx->payload_len, offset, dropItemId, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextIntField fail");
        goto err;
    }

    if(!mapInstance->PickupDropItem(player, dropItemId ,addItemResults))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "PickupDropItem fail");
        goto err;
    }
    

  err:
    if (rc != EXIT_SUCCESS) {
        session->SendNok(PKT_PLAYER_PICKUP_ITEM, errMsg);
    } else {
        ItemPacketSender::SendAddItem(player, addItemResults);
        K_LOG_TRACE( "PickUpItemPacket END");
    }
}



bool TransferData(Str_UseItem& str_itemData, UseItem& itemData)
{
    if(!utility::StringToInt(str_itemData.str_inventoryType , itemData.inventoryType))
    {
        K_LOG_ERROR( "str_inventoryType String to Int fail");
        return false;
    }

    if(!utility::StringToInt(str_itemData.str_useCount , itemData.useCount))
    {
        K_LOG_ERROR( "str_inventoryType String to Int fail");
        return false;
    }

    if(!utility::StringToInt(str_itemData.str_itemId , itemData.itemId))
    {
        K_LOG_ERROR( "str_inventoryType String to Int fail");
        return false;
    }

    if(!utility::StringToInt(str_itemData.str_slotPos , itemData.slotPos))
    {
        K_LOG_ERROR( "str_inventoryType String to Int fail");
        return false;
    }

    K_LOG_DEBUG( "itemData.inventoryType [%d]", itemData.inventoryType);
    K_LOG_DEBUG( "itemData.useCount [%d]", itemData.useCount);
    K_LOG_DEBUG( "itemData.itemId [%d]", itemData.itemId);
    K_LOG_DEBUG( "itemData.slotPos [%d]", itemData.slotPos);

    return true;

}