#include "InventoryManager.h"
#include "Inventory_Info.h"
#include "ItemManager.h"


bool InventoryManager::CreateInventory(InventoryMetaInfo& inventoryMetaInfo)
{
    auto it = m_inventories.find(inventoryMetaInfo.inventoryType);
    if(it != m_inventories.end())
    {
        K_slog_trace(K_SLOG_ERROR, "AllReadyExist");
        return false;
    }

    K_slog_trace(K_SLOG_TRACE, "Success");
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
    auto inventory = m_inventories.find(moveData.inventorytype);
    if (inventory == m_inventories.end())
    {
        errMsg = "inventory is not exist";
        K_slog_trace(K_SLOG_ERROR,"[%s : %s : %d] Inventory is not exist. inventoryType[%d]",__FILE__, __FUNCTION__, __LINE__,moveData.inventorytype);
        return false;
    }

    return inventory->second.MoveItemSlot(moveData,updatedSlots,errMsg);
}

bool InventoryManager::AddItem(int itemId, int count, std::vector<AddItemResult>& addItemResults)
{ 
    auto itemManager = ItemManager::GetInstance();
    if(itemManager == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR,"[%s : %s : %d] itemManager is nullptr",__FILE__, __FUNCTION__,__LINE__);
        return false;
    }

    auto ItemData = itemManager->Find(itemId);

    if(ItemData == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR,"[%s : %s : %d] ItemData is nullptr. itemId[%d]",__FILE__, __FUNCTION__, __LINE__,itemId);
        return false;
    }
    
    int inventoryType = inven::ConvertItemTypeToInventoryType(ItemData->type);

    AddItemData addItemData{};

    addItemData.itemId = itemId;
    addItemData.count = count;
    addItemData.stackable = ItemData->stackable;
    addItemData.max_stack = ItemData->max_stack;

    {
        std::lock_guard<std::mutex> lock(m_inventoryMutex);

        auto iter = m_inventories.find(inventoryType);
        if (iter == m_inventories.end())
        {
            K_slog_trace(K_SLOG_ERROR,
                "[%s : %s : %d] inventory not found. inventoryType[%d]",
                __FILE__, __FUNCTION__, __LINE__, inventoryType);
            return false;
        }

        if (!iter->second.AddItem(addItemData, addItemResults))
        {
            K_slog_trace(K_SLOG_ERROR,
                "[%s : %s : %d] failed to add item. itemId[%d], count[%d], inventoryType[%d]",
                __FILE__, __FUNCTION__, __LINE__, itemId, count, inventoryType);
            return false;
        }
    }

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

void InventoryManager::Clear()
{
    m_inventories.clear();
}


