#include "InventoryPacketSender.h"
#include "ChannelSession.h"
#include "K_slog.h"

void InventoryPacketSender::SendInventoryMeta(Player* player)
{
    auto session = player->GetSession();

    if(!session)
    {
        K_LOG_ERROR( "session이 nullptr입니다.");
        return;
    }

    std::vector<std::string> payload;

    auto inventory = player->GetInventoryManager();

    // inventory size 처음에 넣어줌 클라이언트에서 받을 때 개수를 통해서 검증 시도 및 정확한 개수를 받기 위해서
    payload.push_back(std::to_string(inventory->GetAllMetaInfos().size()));

    for(auto metaInfos : inventory->GetAllMetaInfos())
    {
        payload.push_back(std::to_string(metaInfos.inventoryType));
        payload.push_back(std::to_string(metaInfos.max_slots));
        payload.push_back(std::to_string(metaInfos.currnet_slots_size));
    }

    session->Send(PKT_INVENTORY_META_INFO, payload);
    
}

void InventoryPacketSender::SendInventoryItems(Player* player)
{
    auto session = player->GetSession();

    if(!session)
    {
        K_LOG_ERROR( "session이 nullptr입니다.");
        return;
    }

    auto inventoryManager = player->GetInventoryManager();

    if(inventoryManager == nullptr)
    {
        K_LOG_ERROR( "inventoryManager이 nullptr입니다.");
        return;
    }

    const auto itemInfos = inventoryManager->GetAllItemInfos();

    std::vector<std::string> payload;
    payload.reserve(1 + itemInfos.size() * 4);

    // 아이템 개수 처음에 넣어줌 클라이언트에서 받을 때 검증과 정확한 개수의 아이템을 받기 위해서
    payload.push_back(std::to_string(itemInfos.size()));

    for(auto itemInfo : itemInfos)
    {
        payload.push_back(std::to_string(itemInfo.inventoryType));
        payload.push_back(std::to_string(itemInfo.itemId));
        payload.push_back(std::to_string(itemInfo.itemCount));
        payload.push_back(std::to_string(itemInfo.slotPos));
    }

    session->Send(PKT_INVENTORY_ITEM_INFO, payload);
}

void InventoryPacketSender::SendInventoryMoveItem(ChannelSession* session, bool result, int inventoryType, const std::vector<InventorySlotUpdate>& updatedSlots, const std::string& errMsg)
{
    if (session == nullptr)
        return;

    std::vector<std::string> payload;

    payload.push_back(result ? "1" : "0");
    payload.push_back(std::to_string(inventoryType));
    payload.push_back(std::to_string(updatedSlots.size()));

    for (const auto& slot : updatedSlots)
    {
        payload.push_back(std::to_string(slot.slotPos));
        payload.push_back(std::to_string(slot.itemId));
        payload.push_back(std::to_string(slot.itemCount));
    }

    payload.push_back(errMsg);

    session->Send(PKT_INVENTORY_ITEM_MOVE, payload);
}

