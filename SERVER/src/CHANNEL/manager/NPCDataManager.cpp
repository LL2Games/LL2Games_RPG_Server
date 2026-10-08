#include "NPCDataManager.h"
#include "K_slog.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#define NPC_PATH "../src/CHANNEL/data/NPCs/"

bool NPCDataManager::Init()
{
    namespace fs = std::filesystem;
    std::string currentFile;
    // 모두 읽는 데 성공했을 때만 실제 데이터에 반영한다.
    std::unordered_map<int, NPCDefinition> loaded;
 
    try
    {

        for (const auto& entry : fs::directory_iterator(NPC_PATH))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
            {
                continue;
            }

            currentFile = entry.path().string();
            std::ifstream file(entry.path());

            if (!file.is_open())
                throw std::runtime_error("failed to open NPC file");

            nlohmann::json json;
            file >> json;

            NPCDefinition data{};

            data.npcId = json.at("npcId").get<int>();
            data.name = json.at("name").get<std::string>();
            data.role = json.value("role", std::string{});

            data.dialogue = json.value("dialogue", std::vector<std::string>{});

            if (data.npcId <= 0 || data.name.empty())
                throw std::runtime_error("invalid NPC definition");

            if (json.contains("shop") && !json.at("shop").is_null())
            {
                NPCShopComponent shop{};

                shop.shopId = json.at("shop").at("shopId").get<int>();

                if (shop.shopId <= 0)
                    throw std::runtime_error("invalid shopId");

                data.shop = shop;
            }

            const int npcId = data.npcId;

            if (!loaded.emplace(npcId, std::move(data)).second)
                throw std::runtime_error("duplicated npcId");
        }

        if (loaded.empty())
            throw std::runtime_error("no NPC definitions found");
    }
    catch (const std::exception& exception)
    {
        K_LOG_ERROR("[NPCDataManager] load failed. file[%s] error[%s]",currentFile.c_str(),exception.what());

        return false;
    }

    m_npcs.swap(loaded);
    return true;
}

const NPCDefinition* NPCDataManager::Find(int npcId) const
{
    const auto iter = m_npcs.find(npcId);

    if (iter == m_npcs.end())
        return nullptr;

    return &iter->second;
}