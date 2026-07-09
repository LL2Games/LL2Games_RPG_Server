#include "Inventory.h"


Inventory::Inventory(InventoryMetaInfo& inventoryMetaInfo)
{
    m_inventoryType = inventoryMetaInfo.inventoryType;
    m_maxSlot = inventoryMetaInfo.max_slots;
    m_current_slot_size = inventoryMetaInfo.currnet_slots_size;

    K_LOG_DEBUG( "m_inventoryType = %d", m_inventoryType);
    K_LOG_DEBUG( "m_maxSlot = %d", m_maxSlot);
    K_LOG_DEBUG( "m_current_slot_size = %d", m_current_slot_size);

    m_slots.reserve(m_maxSlot);
    for(int i = 0; i < m_maxSlot; i++)
    {
        InventorySlot slot{};
        slot.slotPos = i;
        slot.isEnable = (i <= m_current_slot_size);
        slot.inventoryType = m_inventoryType;
        slot.itemId = 0;
        slot.itemCount = 0;
        m_slots.emplace(i, slot);
    }
    K_LOG_DEBUG( "Inventory Init Success");
}



// DB에서 아이템 조회 후 저장하는 함수
bool Inventory::SetSlotItem(int slotPos, int itemId, int count)
{
    InventorySlot items;

    items.inventoryType = m_inventoryType;
    items.slotPos = slotPos;
    items.itemId = itemId;
    items.itemCount = count;

    m_slots[slotPos] = items;

    return true;
}

bool Inventory::AddItem(AddItemData& addItemData, std::vector<AddItemResult>& results)
{
    if (addItemData.count <= 0)
        return false;

    int remainCount = addItemData.count;

    // 1. stackable 아이템이면 기존 슬롯부터 순서대로 채운다
    if (addItemData.stackable)
    {
        for (int slotPos = 0; slotPos < m_maxSlot; ++slotPos)
        {
            auto it = m_slots.find(slotPos);
            if (it == m_slots.end())
                continue;

            InventorySlot& slot = it->second;

            if (!slot.isEnable)
                continue;

            if (slot.itemId != addItemData.itemId)
                continue;

            if (slot.itemCount >= addItemData.max_stack)
                continue;

            int canAdd = addItemData.max_stack - slot.itemCount;
            int addCount = std::min(canAdd, remainCount);

            slot.itemCount += addCount;
            remainCount -= addCount;

            AddItemResult result{};
            result.slotPos = slotPos;
            result.itemId = addItemData.itemId;
            result.itemCount = slot.itemCount;
            result.inventoryType = slot.inventoryType;

            results.push_back(result);

            if (remainCount <= 0)
                return true;
        }
    }

    // 2. 남은 수량은 빈 슬롯에 순서대로 넣는다
    for (int slotPos = 0; slotPos < m_maxSlot; ++slotPos)
    {
        auto it = m_slots.find(slotPos);
        if (it == m_slots.end())
            continue;

        InventorySlot& slot = it->second;

        if (!slot.isEnable)
            continue;

        if (slot.itemId != 0)
            continue;

        int addCount = 0;

        if (addItemData.stackable)
            addCount = std::min(addItemData.max_stack, remainCount);
        else
            addCount = 1;

        slot.itemId = addItemData.itemId;
        slot.itemCount = addCount;
        remainCount -= addCount;

        AddItemResult result{};
        result.slotPos = slotPos;
        result.itemId = addItemData.itemId;
        result.itemCount = slot.itemCount;
        result.inventoryType = slot.inventoryType;

        results.push_back(result);
        
        if (remainCount <= 0)
            return true;
    }
    return false;
}

// 아이템 사용 시 아이템 개수 변경
bool Inventory::RemoveItemBySlot(int slotPos, int itemId, int count)
{
    
    K_LOG_DEBUG( "itemData UseCount [%d]", count);
    auto it = m_slots.find(slotPos);

    if(it == m_slots.end())
    {
        K_LOG_ERROR( "플레이어가 가지고 있지 않은 아이템입니다.");
        return false;
    }

    if(it->second.itemId != itemId)
    {
        K_LOG_ERROR( "아이템 ID가 다릅니다. have_itemID=%d 요청 아이템 ID=%d", it->second.itemId, itemId);
        return false;
    }
    
     if (it->second.itemCount < count)
    {
        K_LOG_ERROR( "아이템 개수가 충분하지 않습니다. have=%d need=%d", it->second.itemCount, count);
        return false;
    }

    it->second.itemCount -= count;

    if(it->second.itemCount == 0)
    {
        m_slots.erase(it);
    }

    return true;

}

bool Inventory::HasItemBySlot(int slotPos, int itemId, int count) const
{
    K_LOG_DEBUG( "아이템 사용 개수 [%d]", count);
    auto it = m_slots.find(slotPos);

    if(it == m_slots.end())
    {
        K_LOG_ERROR( "플레이어가 가지고 있지 않은 아이템입니다.");
        return false;
    }

    if(it->second.itemId != itemId)
    {
         K_LOG_ERROR( "아이템 ID가 다릅니다.");
        return false;
    }

    if(it->second.itemCount < count)
    {
        K_LOG_ERROR( "아이템 개수가 충분하지 않습니다. inventoryType=%d slotPos=%d have=%d need=%d", it->second.inventoryType, it->second.slotPos, it->second.itemCount, count);
        return false;
    }

    return true;
}

bool Inventory::HasItemBySlot(int slotPos, int itemId) const
{
   
    auto it = m_slots.find(slotPos);

    if(it == m_slots.end())
    {
        K_LOG_ERROR( "플레이어가 가지고 있지 않은 아이템입니다.");
        return false;
    }

    if(it->second.itemId != itemId)
    {
        K_LOG_ERROR( "[%d][%d] 아이템 ID가 다릅니다.", it->second.itemId, itemId);
        return false;
    }

    return true;
}

InventorySlot* Inventory::FindSlot(int slotPos)
{
    auto it = m_slots.find(slotPos);

    if (it != m_slots.end())
    {
    	return &(it->second);
    }

    return nullptr;
}

int Inventory::GetItemCount(int slotPos, int itemId) const
{

    auto it = m_slots.find(slotPos);

    if(it == m_slots.end())
    {
        K_LOG_ERROR( "플레이어가 가지고 있지 않은 아이템입니다.");
        return 0;
    }

    if(it->second.itemId != itemId)
    {
        K_LOG_ERROR( "아이템 ID가 다릅니다.");
        return 0;
    }

    return it->second.itemCount;
}

std::vector<InventoryItemInfo> Inventory::MakeItemInfos() const
{
    std::vector<InventoryItemInfo> inventoryItemInfos;

    for(auto [slotpos, item] : m_slots)
    {
        InventoryItemInfo iteminfo;

        iteminfo.inventoryType =m_inventoryType;
        iteminfo.slotPos = item.slotPos;
        iteminfo.itemCount =item.itemCount;
        iteminfo.itemId = item.itemId;

        inventoryItemInfos.push_back(iteminfo);
    }
    return inventoryItemInfos;
}

bool Inventory::MoveItemSlot(const MoveItem& moveItem, std::vector<InventorySlotUpdate>& updatedSlots, std::string errMsg)
{
 if (moveItem.fromSlotPos == moveItem.toSlotPos)
    {
        errMsg = "same slot";
        return false;
    }

    InventorySlot* fromSlot = FindSlot(moveItem.fromSlotPos);
    InventorySlot* toSlot = FindSlot(moveItem.toSlotPos);

    if (fromSlot == nullptr)
    {
        errMsg = "from slot not found";
        return false;
    }

    if (toSlot == nullptr)
    {
        errMsg = "to slot not found";
        return false;
    }

    if (fromSlot->itemId == 0)
    {
        errMsg = "from slot is empty";
        return false;
    }

    std::swap(fromSlot->itemId, toSlot->itemId);
    std::swap(fromSlot->itemCount, toSlot->itemCount);

    updatedSlots.push_back({moveItem.fromSlotPos,fromSlot->itemId,fromSlot->itemCount});
    updatedSlots.push_back({moveItem.toSlotPos,toSlot->itemId,toSlot->itemCount});

    return true;
}