#include "QuickSlotPacketHandler.h"
#include "ChannelSession.h"
#include "PacketParser.h"
#include "Slot_Info.h"
#include "utility.h"
#include "QuickSlotManager.h"
#include "Player.h"
#include "QuickSlotPacketSender.h"

void QuickSlotPacketHandler::Execute(PacketContext * ctx)
{
    HandleSetQuickSlot(ctx);
}

void QuickSlotPacketHandler::HandleSetQuickSlot(PacketContext* ctx)
{
    ChannelSession *session = nullptr;
    Player* player = nullptr;
    QuickSlotManager* quickSlotManager = nullptr;
    size_t offset = 0;

   
    int type =0;
    int rc = EXIT_SUCCESS;

    std::string errMsg;
    std::vector<QuickSlotData> result;
    QuickSlotData quickSlotData{};
    
     
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

    quickSlotManager = player->GetQuickSlotManager();
    if(quickSlotManager == nullptr)
    {
        K_LOG_ERROR( "quickSlotManager is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]quickSlotManager is nullptr";
        goto err;
    }

    // 퀵슬롯 인덱스 추출
    if(!PacketParser::ParseNextIntField(ctx->payload,ctx->payload_len,offset,quickSlotData.slot_index,errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextIntField fail");
        goto err;
    }

    if(!PacketParser::ParseNextIntField(ctx->payload,ctx->payload_len,offset,type,errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextIntField fail");
        goto err;
    }
    
    if(!PacketParser::ParseNextIntField(ctx->payload, ctx->payload_len, offset, quickSlotData.ref_id, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextIntField fail");
        goto err;
    }

    if(!PacketParser::ParseNextIntField(ctx->payload, ctx->payload_len, offset, quickSlotData.inventory_type, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextIntField fail");
        goto err;
    }

    if(!PacketParser::ParseNextIntField(ctx->payload, ctx->payload_len, offset, quickSlotData.inventory_slotPos, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextIntField fail");
        goto err;
    }

    if(!PacketParser::ParseNextIntField(ctx->payload, ctx->payload_len, offset, quickSlotData.count, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextIntField fail");
        goto err;
    }
  
    quickSlotData.type = QuickSlot::SetSlotType(type);

    K_LOG_DEBUG( "quickSlotData.type [%d] ", type);
    
    result = quickSlotManager->SetSlot(quickSlotData);

    K_LOG_ERROR(
        "[QuickSlotDebug] index[%d] rawType[%d] mappedType[%d] "
        "refId[%d] resultSize[%zu]",
        quickSlotData.slot_index,
        type,
        static_cast<int>(quickSlotData.type),
        quickSlotData.ref_id,
        result.size()
    );
    if (result.empty())
    {
        rc = EXIT_FAILURE;
        errMsg = "SetSlot failed";
        goto err;
    }
    player->MarkSaveNeeded();
err:
    if (rc != EXIT_SUCCESS) {
        session->SendNok(PKT_QUICKSLOT_SET, errMsg);
    } else {
        QuickSlotPacketSender::SendQuickSlotSet(player, result);
        K_LOG_TRACE( "QuickSlotSet END");
    
    }

}