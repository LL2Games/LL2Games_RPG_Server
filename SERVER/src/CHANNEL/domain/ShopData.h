#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Inventory_Info.h"

struct ShopProduct
{
    int productId = 0; // 상점 안에서 상품을 구분하는 ID
    int itemId = 0;
    std::int64_t price = 0; // 개당 구매 가격
    int maxPerPurchase = 99;
};

struct ShopDefinition
{
    int shopId = 0;
    std::string name;
    bool allowSell = true;

    std::vector<ShopProduct> products;
};

struct ShopBuyResult
{
    int mapId = 0;
    int spawnId = 0;
    int shopId = 0;
    int productId = 0;
    int itemId = 0;
    int count = 0;

    std::int64_t totalPrice = 0;
    std::int64_t gold = 0;

    std::vector<AddItemResult> updatedSlots;
};

struct ShopSellResult
{
    int mapId = 0;
    int spawnId = 0;
    int shopId = 0;

    int itemId = 0;
    int count = 0;

    std::int64_t totalPrice = 0;
    std::int64_t gold = 0;

    AddItemResult updatedSlot{};
};