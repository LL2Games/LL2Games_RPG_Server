#include "ShopService.h"

#include "NPCInteractionService.h"
#include "ShopManager.h"
#include "Player.h"
#include "MapInstance.h"
#include "InventoryManager.h"
#include "ItemManager.h"
#include "GameplayGate.h"
#include "K_slog.h"

#include <limits>
#include <utility>

namespace
{
    // 지급 실패 또는 예외 발생 시 차감한 재화를 복구한다.
    class GoldRefundGuard
    {
    public:
        GoldRefundGuard(Player& player, std::int64_t amount)
            : m_player(player), m_amount(amount)
        {
        }

        ~GoldRefundGuard()
        {
            if (m_active && !m_player.TryAddGold(m_amount))
            {
                K_LOG_ERROR("[ShopService] gold refund failed. playerId[%d]",m_player.GetId());
            }
        }

        void Commit() noexcept
        {
            m_active = false;
        }

        GoldRefundGuard(const GoldRefundGuard&) = delete;
        GoldRefundGuard& operator=(const GoldRefundGuard&) = delete;

    private:
        Player& m_player;
        std::int64_t m_amount;
        bool m_active = true;
    };

    class GoldCreditRollbackGuard
    {
    public:
        GoldCreditRollbackGuard(Player& player, std::int64_t amount)
            : m_player(player), m_amount(amount)
        {
        }

        ~GoldCreditRollbackGuard()
        {
            if (m_active && !m_player.TrySpendGold(m_amount))
            {
                K_LOG_ERROR(
                    "[ShopService] sale rollback failed. playerId[%d]",
                    m_player.GetId());
            }
        }

        void Commit() noexcept
        {
            m_active = false;
        }

        GoldCreditRollbackGuard(const GoldCreditRollbackGuard&) = delete;

        GoldCreditRollbackGuard& operator=(
            const GoldCreditRollbackGuard&) = delete;

    private:
        Player& m_player;
        std::int64_t m_amount;
        bool m_active = true;
    };
}

bool ShopService::Buy(Player* player, int mapId, int spawnId, int productId, int count, ShopBuyResult& outResult, std::string& errMsg) const
{
    outResult = {};
    errMsg.clear();

    // 검증부터 재화·아이템 반영까지 한 구간으로 처리한다.
    Gameplay::Guard guard(Gameplay::gate);

    if (m_npcService == nullptr || m_shopManager == nullptr)
    {
        errMsg = "shop service unavailable";
        return false;
    }

    if (player == nullptr)
    {
        errMsg = "player not found";
        return false;
    }

    MapInstance* map = player->GetCurrentMap();

    if (mapId <= 0 || map == nullptr || map->GetMapId() != static_cast<std::uint32_t>(mapId))
    {
        errMsg = "map mismatch";
        return false;
    }

    if (productId <= 0 || count <= 0)
    {
        errMsg = "invalid purchase request";
        return false;
    }

    // 구매할 때도 NPC 존재·생존·거리 조건을 다시 확인한다.
    NPCInteractionResult npc{};

    if (!m_npcService->Interact(player, spawnId, npc, errMsg))
        return false;

    if (npc.shopId <= 0)
    {
        errMsg = "npc has no shop";
        return false;
    }

    const ShopDefinition* shop = m_shopManager->Find(npc.shopId);

    if (shop == nullptr)
    {
        errMsg = "shop not found";
        return false;
    }

    const ShopProduct* product = nullptr;

    for (const ShopProduct& candidate : shop->products)
    {
        if (candidate.productId == productId)
        {
            product = &candidate;
            break;
        }
    }

    if (product == nullptr)
    {
        errMsg = "product not found";
        return false;
    }

    if (count > product->maxPerPurchase)
    {
        errMsg = "purchase count exceeds limit";
        return false;
    }

    // 곱셈 전에 검사한다.
    if (product->price <= 0 || product->price > std::numeric_limits<std::int64_t>::max() / count)
    {
        errMsg = "invalid total price";
        return false;
    }

    const std::int64_t totalPrice = product->price * count;

    InventoryManager* inventory = player->GetInventoryManager();

    if (inventory == nullptr)
    {
        errMsg = "inventory unavailable";
        return false;
    }

    ShopBuyResult result{};
    result.mapId = mapId;
    result.spawnId = spawnId;
    result.shopId = shop->shopId;
    result.productId = productId;
    result.itemId = product->itemId;
    result.count = count;
    result.totalPrice = totalPrice;

    if (!player->TrySpendGold(totalPrice))
    {
        errMsg = "not enough gold";
        return false;
    }

    GoldRefundGuard refund(*player, totalPrice);

    result.gold = player->GetGold();

    // 앞 단계에서 수정한 AddItem은 실패 시 원본을 유지한다.
    if (!inventory->AddItem(product->itemId, count, result.updatedSlots))
    {
        errMsg = "failed to add purchased items";
        return false; // refund 소멸자가 재화를 복구한다.
    }

    refund.Commit();
    outResult = std::move(result);

    return true;
}

bool ShopService::Sell(Player* player, int mapId, int spawnId, int inventoryType, int slotPos, int count, ShopSellResult& outResult, std::string& errMsg) const
{
    outResult = {};
    errMsg.clear();

    Gameplay::Guard guard(Gameplay::gate);

    if (m_npcService == nullptr || m_shopManager == nullptr)
    {
        errMsg = "shop service unavailable";
        return false;
    }

    if (player == nullptr)
    {
        errMsg = "player not found";
        return false;
    }

    MapInstance* map = player->GetCurrentMap();

    if (mapId <= 0 || map == nullptr || map->GetMapId() != static_cast<std::uint32_t>(mapId))
    {
        errMsg = "map mismatch";
        return false;
    }

    if (slotPos < 0 || count <= 0)
    {
        errMsg = "invalid sale request";
        return false;
    }

    NPCInteractionResult npc{};

    if (!m_npcService->Interact(player, spawnId, npc, errMsg))
        return false;

    if (npc.shopId <= 0)
    {
        errMsg = "npc has no shop";
        return false;
    }

    const ShopDefinition* shop = m_shopManager->Find(npc.shopId);

    if (shop == nullptr || !shop->allowSell)
    {
        errMsg = "shop does not accept sales";
        return false;
    }

    InventoryManager* inventory = player->GetInventoryManager();
    ItemManager* itemManager = ItemManager::GetInstance();

    if (inventory == nullptr || itemManager == nullptr)
    {
        errMsg = "item service unavailable";
        return false;
    }

    InventorySlot slot{};

    if (!inventory->GetSlotSnapshot(inventoryType, slotPos, slot))
    {
        errMsg = "inventory item not found";
        return false;
    }

    if (count > slot.itemCount)
    {
        errMsg = "not enough items";
        return false;
    }

    const ItemInitData* item = itemManager->Find(slot.itemId);

    if (item == nullptr || inven::ConvertItemTypeToInventoryType(item->type) != inventoryType)
    {
        errMsg = "invalid item definition";
        return false;
    }

    const std::int64_t unitPrice = item->sell_price;

    if (unitPrice <= 0)
    {
        errMsg = "item cannot be sold";
        return false;
    }

    if (unitPrice > std::numeric_limits<std::int64_t>::max() / count)
    {
        errMsg = "invalid total sale price";
        return false;
    }

    const std::int64_t totalPrice = unitPrice * count;
    const int remainingCount = slot.itemCount - count;

    ShopSellResult result{};
    result.mapId = mapId;
    result.spawnId = spawnId;
    result.shopId = shop->shopId;
    result.itemId = slot.itemId;
    result.count = count;
    result.totalPrice = totalPrice;

    result.updatedSlot.inventoryType = inventoryType;
    result.updatedSlot.slotPos = slotPos;
    result.updatedSlot.itemId = remainingCount > 0 ? slot.itemId : 0;
    result.updatedSlot.itemCount = remainingCount;

    if (!player->TryAddGold(totalPrice))
    {
        errMsg = "gold balance limit exceeded";
        return false;
    }

    GoldCreditRollbackGuard rollback(*player, totalPrice);
    result.gold = player->GetGold();

    if (!inventory->RemoveItemBySlot(inventoryType, slotPos, slot.itemId, count))
    {
        errMsg = "failed to remove sold items";
        return false; // 지급한 재화를 다시 회수한다.
    }

    rollback.Commit();
    outResult = std::move(result);

    return true;
}