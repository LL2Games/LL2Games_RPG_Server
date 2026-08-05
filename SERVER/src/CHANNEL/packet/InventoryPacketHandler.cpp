#include "InventoryPacketHandler.h"
#include "ChannelSession.h"
#include "PacketParser.h"
#include "InventoryManager.h"
#include "InventoryPacketSender.h"
#include "Player.h"


void InventoryPacketHandler::Execute(PacketContext * ctx)
{
    ChannelSession *session = nullptr;
    Player* player = nullptr;
    InventoryManager* inventoryManager = nullptr;
    int rc = EXIT_SUCCESS;
    
    size_t offset = 0;
    std::string errMsg;
    std::string str_inventoryType;
    std::string str_fromSlotPos;
    std::string str_toSlotPos;
    bool result = false;

    std::vector<InventorySlotUpdate> inventorySlotUpdate;

    MoveItem moveData{};

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
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
   }

   inventoryManager = player->GetInventoryManager();

   if(inventoryManager == nullptr)
   {
        K_LOG_ERROR( "inventoryManager is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]inventoryManager is nullptr";
        goto err;
   }
   // inventoryType 
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

    //fromSlotPos 
    if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        str_fromSlotPos,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    //toSlotPos 
    if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        str_toSlotPos,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    if(!utility::StringToInt(str_inventoryType, moveData.inventorytype))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "playerid String To Int Fail");
        goto err;
    }

    if(!utility::StringToInt(str_fromSlotPos, moveData.fromSlotPos))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "playerid String To Int Fail");
        goto err;
    }

     if(!utility::StringToInt(str_toSlotPos, moveData.toSlotPos))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "playerid String To Int Fail");
        goto err;
    }
   
   result = inventoryManager->MoveItemSlots(moveData, inventorySlotUpdate, errMsg);
   // InventoryManager에서 인벤토리 슬롯 움직이기 함수 실행
   if(!result)
   {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "MoveItemSlots Err [%s]", errMsg);
        goto err;
   }
   player->MarkSaveNeeded();
   K_LOG_TRACE( "InventoryPacketHandler ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ");
err:
    if (rc != EXIT_SUCCESS) {
        session->SendNok(PKT_INVENTORY_ITEM_MOVE, errMsg);
    } else {
        K_LOG_TRACE( "PKT_INVENTORY_ITEM_MOVE END");
        InventoryPacketSender::SendInventoryMoveItem(session,result, moveData.inventorytype, inventorySlotUpdate, errMsg);
    }
}