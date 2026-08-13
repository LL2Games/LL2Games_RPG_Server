#include "ItemService.h"
#include "ChannelSession.h"
#include "PacketDTO.h"

ItemService::ItemService()
{
    m_item_manager = ItemManager::GetInstance();
}


int ItemService::HandleUseItem(Player* player, UseItem itemData, UseItemResult& result)
{
    int is_use; 
    K_LOG_DEBUG( "아이템 사용 개수 [%d]\n", itemData.useCount);
    if(!player->CanUseItem(itemData.inventoryType, itemData.slotPos, itemData.itemId, itemData.useCount))
    {
        K_LOG_ERROR( "[%d] 아이템 사용 불가\n", itemData.itemId);
        return EXIT_FAILURE;
    }

    is_use = player->UseItem(itemData.inventoryType, itemData.slotPos, itemData.itemId, itemData.useCount);

    if(!is_use)
    {
        result.result = false;
        return false;
    }
    // 해당 플레이어한테 체력 변경 전달 및 아이템 사용 동기화
    // is_use가 현재 bool 형인데 나중에 err_code 보내는 것으로 변경 해야함 
    result.result = is_use;
    result.hp = player->GetCurHP();
    result.mp = player->GetCurMP();
    result.inventoryType = itemData.inventoryType;
    result.slotPos = itemData.slotPos;
    result.item_id = itemData.itemId;
    result.used_count = itemData.useCount;
    // 잠재적 에러 발생 - 수정 필요
    result.remain_count = player->GetItemCount(itemData.inventoryType, itemData.slotPos, itemData.itemId);
    return EXIT_SUCCESS;
}




