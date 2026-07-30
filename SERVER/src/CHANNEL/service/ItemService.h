#pragma once
#include "common.h"
#include "ItemManager.h"
#include "Player.h"
#include "CommonEnum.h"
#include "PacketDTO.h"

class ItemService
{
public:
    ItemService();
    ~ItemService(){};

public:
    const ItemInitData* Find(int itemID);
    int HandleUseItem(Player* player, UseItem itemData, UseItemResult& result);
private:
    ItemManager* m_item_manager;
};
