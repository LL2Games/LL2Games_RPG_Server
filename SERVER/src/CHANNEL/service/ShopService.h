#pragma once

#include "ShopData.h"
#include <string>

class Player;
class NPCInteractionService;
class ShopManager;

class ShopService
{
public:
    void Setup(const NPCInteractionService* npcService, const ShopManager* shopManager)
    {
        m_npcService = npcService;
        m_shopManager = shopManager;
    }

    bool Buy(Player* player, int mapId, int spawnId, int productId, int count, ShopBuyResult& outResult, std::string& errMsg) const;
    bool Sell(Player* player, int mapId, int spawnId, int inventoryType, int slotPos, int count, ShopSellResult& outResult, std::string& errMsg) const;
private:
    const NPCInteractionService* m_npcService = nullptr;
    const ShopManager* m_shopManager = nullptr;
};