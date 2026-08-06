#pragma once
#include "common.h"
#include "Inventory.h"
#include <mutex>

struct InventorySaveData
{
    std::vector<InventoryMetaInfo> metaInfos;
    std::vector<InventoryItemInfo> itemInfos;
};

class InventoryManager
{
public:
    
	bool CreateInventory(InventoryMetaInfo& inventoryMetaInfo);
	void EnsureInventory(InventoryMetaInfo& inventoryMetaInfo);
	bool MoveItemSlots(const MoveItem& moveData,std::vector<InventorySlotUpdate>& updatedSlots,std::string& errMsg);

	bool AddItem(int itemId, int count, std::vector<AddItemResult>& addItemResults);

	bool HasItemBySlot(int inventoryType, int slotPos, int itemId, int count) const;
	bool RemoveItemBySlot(int inventoryType, int slotPos, int itemId, int count);
	int GetItemCount(int inventoryType, int slotPos, int itemId) const;
public:
	Inventory* GetInventory(int inventoryType);
	const Inventory* GetInventory(int inventoryType) const;

	InventorySlot* FindSlot(int inventoryType, int slotPos);
	//const InventorySlot* FindSlot(int inventoryType, int slotPos) const;

	std::vector<InventoryMetaInfo> GetAllMetaInfos() const;
	std::vector<InventoryItemInfo> GetAllItemInfos() const;

	InventorySaveData MakeSaveInventoryData() const;

	void Clear();
private:
    std::unordered_map<int, Inventory> m_inventories;
	mutable std::mutex m_inventoryMutex;
};