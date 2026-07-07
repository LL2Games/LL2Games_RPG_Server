#pragma once
#include "common.h"
#include "Inventory.h"
#include <mutex>

class InventoryManager
{
public:
    
	bool CreateInventory(InventoryMetaInfo& inventoryMetaInfo);
	void EnsureInventory(InventoryMetaInfo& inventoryMetaInfo);
	bool MoveItemSlots(const MoveItem& moveData,std::vector<InventorySlotUpdate>& updatedSlots,std::string& errMsg);

	bool AddItem(int itemId, int count, std::vector<AddItemResult>& addItemResults);
public:
	Inventory* GetInventory(int inventoryType);
	const Inventory* GetInventory(int inventoryType) const;

	InventorySlot* FindSlot(int inventoryType, int slotPos);
	//const InventorySlot* FindSlot(int inventoryType, int slotPos) const;

	std::vector<InventoryMetaInfo> GetAllMetaInfos() const;
	std::vector<InventoryItemInfo> GetAllItemInfos() const;

	void Clear();
private:
    std::unordered_map<int, Inventory> m_inventories;
	std::mutex m_inventoryMutex;
};