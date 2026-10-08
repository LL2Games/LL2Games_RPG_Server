#pragma once

#include "CharacterStat.h"
#include "Inventory_Info.h"
#include "Slot_Info.h"
#include "CommonEnum.h"

#include <cstdint>
#include <vector>

struct PlayerSaveData
{
    std::uint64_t saveVersion = 0;

    int characterId = 0;
    int mapId = 0;
    Vec2 position{};
    std::int64_t gold = 0;

    CharacterStat stat{};

    std::vector<InventoryMetaInfo> inventoryMetas;
    std::vector<InventoryItemInfo> inventoryItems;
    std::vector<QuickSlotData> quickSlots;
};