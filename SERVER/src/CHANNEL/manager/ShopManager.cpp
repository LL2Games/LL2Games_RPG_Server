#include "ShopManager.h"
#include "ItemManager.h"
#include "K_slog.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

#define SHOP_PATH "../src/CHANNEL/data/Shops/"

bool ShopManager::Init()
{
    namespace fs = std::filesystem;

    std::unordered_map<int, ShopDefinition> loaded;
    std::string currentFile = SHOP_PATH;

    try
    {
        ItemManager* itemManager = ItemManager::GetInstance();

        if (itemManager == nullptr)
            throw std::runtime_error("item manager unavailable");

        for (const auto& entry : fs::directory_iterator(SHOP_PATH))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
            {
                continue;
            }

            currentFile = entry.path().string();
            std::ifstream file(entry.path());

            if (!file.is_open())
                throw std::runtime_error("failed to open shop file");

            nlohmann::json json;
            file >> json;

            ShopDefinition shop{};

            shop.shopId = json.at("shopId").get<int>();
            shop.name = json.at("name").get<std::string>();
            shop.allowSell = json.value("allowSell", true);

            if (shop.shopId <= 0 || shop.name.empty())
                throw std::runtime_error("invalid shop definition");

            const auto& products = json.at("products");

            if (!products.is_array())
                throw std::runtime_error("products must be an array");

            std::unordered_set<int> productIds;

            for (const auto& productJson : products)
            {
                ShopProduct product{};

                product.productId = productJson.at("productId").get<int>();
                product.itemId = productJson.at("itemId").get<int>();
                product.price = productJson.at("price").get<std::int64_t>();
                product.maxPerPurchase = productJson.value("maxPerPurchase", 99);

                if (product.productId <= 0 ||
                    product.itemId <= 0 ||
                    product.price <= 0 ||
                    product.maxPerPurchase <= 0)
                {
                    throw std::runtime_error("invalid shop product");
                }

                if (!productIds.insert(product.productId).second)
                    throw std::runtime_error("duplicated productId");

                if (itemManager->Find(product.itemId) == nullptr)
                    throw std::runtime_error("shop item not found");

                shop.products.push_back(product);
            }

            const int shopId = shop.shopId;

            if (!loaded.emplace(shopId, std::move(shop)).second)
                throw std::runtime_error("duplicated shopId");
        }

        if (loaded.empty())
            throw std::runtime_error("no shop definitions found");
    }
    catch (const std::exception& exception)
    {
        K_LOG_ERROR(
            "[ShopManager] load failed. file[%s] error[%s]",
            currentFile.c_str(),
            exception.what());

        return false;
    }

    m_shops.swap(loaded);
    return true;
}

const ShopDefinition* ShopManager::Find(int shopId) const
{
    const auto iter = m_shops.find(shopId);

    if (iter == m_shops.end())
        return nullptr;

    return &iter->second;
}