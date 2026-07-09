#include "QuickSlotPacketSender.h"
#include "ChannelSession.h"


void QuickSlotPacketSender::SendQuickSlotList(Player* player)
{
    auto session = player->GetSession();

    if(!session) 
    {
        K_LOG_ERROR( "session이 nullptr입니다.");
        return;
    }

    auto quickSlotManager = player->GetQuickSlotManager();
    auto quickSlotList = quickSlotManager->GetSlotList();
    std::vector<std::string> payload;

    payload.push_back(std::to_string(quickSlotList.size()));
    // 퀵슬롯 정보 전달
    for(auto quickSlot : quickSlotList)
    {
        payload.push_back(std::to_string(quickSlot.slot_index));
        payload.push_back(std::to_string(static_cast<int>(quickSlot.type)));
        payload.push_back(std::to_string(quickSlot.ref_id));
        payload.push_back(std::to_string(quickSlot.inventory_type));
        payload.push_back(std::to_string(quickSlot.inventory_slotPos));
    }

    session->Send(PKT_QUICKSLOT_LIST, payload);

    K_LOG_TRACE( "SendQuickSlotList Send Success.");
}

void QuickSlotPacketSender::SendQuickSlotSet(Player* player, std::vector<QuickSlotData>& result)
{
    auto session = player->GetSession();

    if(!session) 
    {
        K_LOG_ERROR( "session이 nullptr입니다.");
        return;
    }

    std::vector<std::string> payload;
    payload.push_back(std::to_string(result.size()));

    for(const auto& slot : result)
    {
        payload.push_back(std::to_string(slot.slot_index));
        payload.push_back(std::to_string(static_cast<int>(slot.type)));
        payload.push_back(std::to_string(slot.ref_id));
        payload.push_back(std::to_string(static_cast<int>(slot.inventory_type)));
        payload.push_back(std::to_string(slot.inventory_slotPos));
        payload.push_back(std::to_string(slot.count));
    }
   

    session->Send(PKT_QUICKSLOT_SET, payload);

    K_LOG_TRACE( "PKT_QUICKSLOT_SET Send Success.");
}