#include "InventoryManager.h"
#include "Inventory_Info.h"
#include "ItemManager.h"


bool InventoryManager::CreateInventory(InventoryMetaInfo& inventoryMetaInfo)
{
    auto it = m_inventories.find(inventoryMetaInfo.inventoryType);
    if(it != m_inventories.end())
    {
        K_LOG_ERROR( "AllReadyExist");
        return false;
    }

    //K_LOG_TRACE("Success");
    m_inventories.emplace(inventoryMetaInfo.inventoryType, Inventory(inventoryMetaInfo));

    return true;
}

void InventoryManager::EnsureInventory(InventoryMetaInfo& inventoryMetaInfo)
{
    auto it = m_inventories.find(inventoryMetaInfo.inventoryType);
    if (it == m_inventories.end())
    {
    	m_inventories.emplace(inventoryMetaInfo.inventoryType, Inventory(inventoryMetaInfo));
    }
}

bool InventoryManager::MoveItemSlots(const MoveItem& moveData,std::vector<InventorySlotUpdate>& updatedSlots,std::string& errMsg)
{
    std::lock_guard<std::mutex> lock(m_inventoryMutex);
    auto inventory = m_inventories.find(moveData.inventorytype);
    if (inventory == m_inventories.end())
    {
        errMsg = "inventory is not exist";
        K_LOG_ERROR( "Inventory is not exist. inventoryType[%d]", moveData.inventorytype);
        return false;
    }

    return inventory->second.MoveItemSlot(moveData,updatedSlots,errMsg);
}

bool InventoryManager::AddItem(int itemId, int count, std::vector<AddItemResult>& addItemResults)
{ 
    if (itemId <= 0 || count <= 0)
        return false;

    auto itemManager = ItemManager::GetInstance();

    if(itemManager == nullptr)
    {
        K_LOG_ERROR( "itemManager is nullptr");
        return false;
    }

    auto ItemData = itemManager->Find(itemId);

    if(ItemData == nullptr)
    {
        K_LOG_ERROR( "ItemData is nullptr. itemId[%d]", itemId);
        return false;
    }

    if (ItemData->stackable && ItemData->max_stack <= 0)
    {
        K_LOG_ERROR("[AddItem] invalid max_stack. itemId[%d] max_stack[%d]",itemId, ItemData->max_stack);
        return false;
    }
       
    
    const int inventoryType = inven::ConvertItemTypeToInventoryType(ItemData->type);

    if (inventoryType == inven::Invalid)
    {
        K_LOG_ERROR("[AddItem] invalid item type. itemId[%d] type[%s]",itemId, ItemData->type.c_str());
        return false;
    }
        

    AddItemData addItemData{};
    addItemData.itemId = itemId;
    addItemData.count = count;
    addItemData.stackable = ItemData->stackable;
    addItemData.max_stack = ItemData->max_stack;

    
    std::lock_guard<std::mutex> lock(m_inventoryMutex);

    auto iter = m_inventories.find(inventoryType);
    if (iter == m_inventories.end())
    {
        K_LOG_ERROR("[AddItem] inventory not found. itemId[%d] inventoryType[%d]",itemId, inventoryType);
        return false;
    }
       

    Inventory candidate = iter->second;
    std::vector<AddItemResult> pendingResults;

    if (!candidate.AddItem(addItemData, pendingResults))
    {
        K_LOG_ERROR(
        "[AddItem] slot insertion failed. "
        "itemId[%d] count[%d] inventoryType[%d] "
        "maxSlots[%d] currentSlots[%d] maxStack[%d]",
        itemId,
        count,
        inventoryType,
        iter->second.GetMaxSlotSize(),
        iter->second.GetCurrentSlotSize(),
        ItemData->max_stack);
        // 실패한 복사본은 버린다.
        // 원본과 호출자의 결과 목록은 변경되지 않는다.
        return false;
    }
    
 // 결과 목록의 추가까지 성공한 뒤 원본에 반영한다.
    addItemResults.insert(addItemResults.end(),pendingResults.begin(),pendingResults.end());

    iter->second.Swap(candidate);

    return true;
}

Inventory* InventoryManager::GetInventory(int inventoryType) 
{
    auto it = m_inventories.find(inventoryType);
    if (it == m_inventories.end())
    {
    	return nullptr;
    }

    return &(it->second);
}

const Inventory* InventoryManager::GetInventory(int inventoryType) const
{
    auto it = m_inventories.find(inventoryType);
    if (it == m_inventories.end())
    {
    	return nullptr;
    }

    return &(it->second);
}

InventorySlot* InventoryManager::FindSlot(int inventoryType, int slotPos)
{

    auto it = m_inventories.find(inventoryType);

    if (it == m_inventories.end())
    {
    	return nullptr;
    }
    return it->second.FindSlot(slotPos);
}

std::vector<InventoryMetaInfo> InventoryManager::GetAllMetaInfos() const
{
    std::vector<InventoryMetaInfo> inventoryMetaInfos;
     std::lock_guard<std::mutex> lock(m_inventoryMutex);
    for(auto [type, inventory] : m_inventories)
    {
        InventoryMetaInfo inventorymetaInfo;
        inventorymetaInfo.inventoryType = inventory.GetInventoryType();
        inventorymetaInfo.max_slots = inventory.GetMaxSlotSize();
        inventorymetaInfo.currnet_slots_size = inventory.GetCurrentSlotSize();

        inventoryMetaInfos.push_back(inventorymetaInfo);
    }

    return inventoryMetaInfos;
}

std::vector<InventoryItemInfo> InventoryManager::GetAllItemInfos() const
{
    std::vector<InventoryItemInfo> inventoryItemInfos;

     std::lock_guard<std::mutex> lock(m_inventoryMutex);
    for(auto [type, inventory] : m_inventories)
    {
         std::vector<InventoryItemInfo> items = inventory.MakeItemInfos();

         inventoryItemInfos.insert(
            inventoryItemInfos.end(),
            items.begin(),
            items.end()
         );
    }
    return inventoryItemInfos;
}

bool InventoryManager::GetSlotSnapshot(int inventoryType, int slotPos, InventorySlot& outSlot)
{
    outSlot = {};

    std::lock_guard<std::mutex> lock(m_inventoryMutex);

    const auto iter = m_inventories.find(inventoryType);

    if (iter == m_inventories.end())
        return false;

    const InventorySlot* slot = iter->second.FindSlot(slotPos);

    if (slot == nullptr || !slot->isEnable || slot->itemId <= 0 || slot->itemCount <= 0)
    {
        return false;
    }

    outSlot = *slot;
    return true;
}

void InventoryManager::Clear()
{
    std::lock_guard<std::mutex> lock(m_inventoryMutex);
    m_inventories.clear();
}

InventorySaveData InventoryManager::MakeSaveInventoryData() const
{
    std::lock_guard<std::mutex> lock(m_inventoryMutex);

    InventorySaveData saveData{};
    saveData.metaInfos.reserve(m_inventories.size());

    for (const auto& [inventoryType, inventory] : m_inventories)
    {
        InventoryMetaInfo metaInfo{};
        metaInfo.inventoryType = inventory.GetInventoryType();
        metaInfo.max_slots = inventory.GetMaxSlotSize();
        metaInfo.currnet_slots_size = inventory.GetCurrentSlotSize();

        saveData.metaInfos.push_back(metaInfo);

        const std::vector<InventoryItemInfo> itemInfos = inventory.MakeItemInfos();

        saveData.itemInfos.insert(saveData.itemInfos.end(),itemInfos.begin(),itemInfos.end());

        (void)inventoryType;
    }

    return saveData;
}


bool InventoryManager::HasItemBySlot(int inventoryType, int slotPos, int itemId, int count) const
{
    std::lock_guard<std::mutex> lock(m_inventoryMutex);

    const auto inventory = m_inventories.find(inventoryType);

    if (inventory == m_inventories.end())
    {
        return false;
    }

    return inventory->second.HasItemBySlot(slotPos,itemId,count);
}
bool InventoryManager::RemoveItemBySlot(int inventoryType, int slotPos, int itemId, int count)
{
    if (count <= 0)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_inventoryMutex);

    auto inventory = m_inventories.find(inventoryType);

    if (inventory == m_inventories.end())
    {
        return false;
    }

    return inventory->second.RemoveItemBySlot(slotPos,itemId,count);
}
int InventoryManager::GetItemCount(int inventoryType, int slotPos, int itemId) const
{
    std::lock_guard<std::mutex> lock(m_inventoryMutex);

    const auto inventory = m_inventories.find(inventoryType);

    if (inventory == m_inventories.end())
    {
        return 0;
    }

    return inventory->second.GetItemCount(slotPos,itemId);
}