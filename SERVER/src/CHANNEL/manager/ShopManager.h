#pragma once

#include "ShopData.h"
#include <unordered_map>

class ShopManager
{
public:
    static ShopManager* GetInstance()
    {
        static ShopManager instance;
        return &instance;
    }

    bool Init();
    const ShopDefinition* Find(int shopId) const;

    ShopManager(const ShopManager&) = delete;
    ShopManager& operator=(const ShopManager&) = delete;

private:
    ShopManager() = default;

    std::unordered_map<int, ShopDefinition> m_shops;
};