#include "DropManager.h"

#define DROPDATA_PATH "../src/CHANNEL/data/Drops/"

DropManager *DropManager::m_instance = nullptr;

DropManager::DropManager() : m_gen(std::random_device{}())
{

}

DropManager *DropManager::GetInstance()
{
     if(m_instance == nullptr)
    {
        m_instance = new DropManager();
        //if(m_instance->Init() != EXIT_SUCCESS)
        //{
        //    delete m_instance;
        //    m_instance = nullptr;
        //}
    }
    return m_instance;
}

bool DropManager::Init()
{
    if(!PreLoadAll()) return false;
    return true;
}

bool DropManager::PreLoadAll()
{
    for(const auto &entry : fs::directory_iterator(DROPDATA_PATH))
    {
        if(!entry.is_regular_file())
            continue;
        if(entry.path().extension() != ".json")
            continue;

        std::string filename = entry.path().stem().string();

        if(filename == "monster_drop_groups_common")
        {
            bool is_commonLoad = LoadCommonDropFile(entry.path());
            if(!is_commonLoad) return false;
        }else if(filename == "monster_drop_groups_unique")
        {
            bool is_uniqueLoad = LoadUniqueDropFile(entry.path());
            if(!is_uniqueLoad) return false;
        }        
    }
    K_LOG_TRACE( "DropManager PreLoadAll Success");
    return true;
}

bool DropManager::LoadCommonDropFile(const fs::path& path)
{
   
    std::ifstream file(path);

    if(!file.is_open())
    {
        K_LOG_ERROR( "FAILED OPEN [%s] FILE", path.c_str());
        return false;
    }

    nlohmann::json j;
  
    try
    {
        file >> j;
    }
    catch (const std::exception& e)
    {
        K_LOG_ERROR( "JSON PARSE ERROR [%s] : %s", path.string().c_str(), e.what());
        return false;
    }

    if(!j.contains("drop_groups") || !j["drop_groups"].is_object())
    {
        K_LOG_ERROR( "INVALID drop_groups [%s]", path.string().c_str());
        return false;
    }

    const auto& groups = j["drop_groups"];

    for(auto it = groups.begin(); it != groups.end(); ++it)
    {
        const std::string groupId = it.key();
        const auto& groupJson = it.value();

        DropGroup group;
        group.groupId = groupId;
        group.minDrop = groupJson.value("min_drop",0);
        group.maxDrop = groupJson.value("max_drop",0);
        group.allowDuplicate = groupJson.value("allow_duplicate", false);

        if (!groupJson.contains("entries") || !groupJson["entries"].is_array())
        {
            K_LOG_ERROR( "INVALID entries in group [%s]", groupId.c_str());
            return false;
        }

        for(const auto& entryJson : groupJson["entries"])
        {
            DropEntry entry;

            std::string type = entryJson.value("type", "");
            if(type == "item")
            {
                entry.type = DropType::Item;
                entry.itemId = entryJson.value("item_id", 0);
                entry.weight = entryJson.value("weight",0);
                entry.minCount = entryJson.value("min_count",0);
                entry.maxCount = entryJson.value("max_count",0);
                entry.tag = entryJson.value("tag", "");
            }
            else
            {
                K_LOG_ERROR( "UNKNOWN entry type in group [%s]", groupId.c_str());
                return false;
            }
            group.entries.push_back(entry);
        }
        K_LOG_TRACE( "common groupId [%s]", group.groupId.c_str());
        m_commonGroups[group.groupId] = group;
    }   
    return true;

}


bool DropManager::LoadUniqueDropFile(const fs::path& path)
{
    std::ifstream file(path);

    if(!file.is_open())
    {
        K_LOG_ERROR( "FAILED OPEN [%s] FILE", path.c_str());
        return false;
    }

    nlohmann::json j;

    try
    {
        file >> j;
    }
    catch (const std::exception& e)
    {
        K_LOG_ERROR( "JSON PARSE ERROR [%s] : %s", path.string().c_str(), e.what());
        return false;
    }

    if(!j.contains("drop_groups") || !j["drop_groups"].is_object())
    {
        K_LOG_ERROR( "INVALID drop_groups [%s]", path.string().c_str());
        return false;
    }

    const auto& groups = j["drop_groups"];

    for(auto it = groups.begin(); it != groups.end(); ++it)
    {
        const std::string groupId = it.key();
        const auto& groupJson = it.value();

        DropGroup group;
        group.groupId = groupId;
        group.minDrop = groupJson.value("min_drop",0);
        group.maxDrop = groupJson.value("max_drop",0);
        group.allowDuplicate = groupJson.value("allow_duplicate", false);

        if (!groupJson.contains("entries") || !groupJson["entries"].is_array())
        {
            K_LOG_ERROR( "INVALID entries in group [%s]", groupId.c_str());
            return false;
        }

        for(const auto& entryJson : groupJson["entries"])
        {
            DropEntry entry;

            entry.type = DropType::Item;
            entry.itemId = entryJson.value("item_id", 0);
            entry.weight = entryJson.value("weight",0);
            entry.minCount = entryJson.value("min_count",0);
            entry.maxCount = entryJson.value("max_count",0);
            entry.tag = entryJson.value("tag", "");
           
            group.entries.push_back(entry);
        }
        m_uniqueGroups[group.groupId] = group;
    }   
    return true;

}


std::vector<DropResult> DropManager::SetDropItem(std::string& commonGroup, std::string& uniqueGroup)
{
   
    std::vector<DropResult> dropItems{};
    std::vector<DropEntry> common_entries{};

    auto common_it = m_commonGroups.find(commonGroup);
    auto unique_it = m_uniqueGroups.find(uniqueGroup);
    if(common_it == m_commonGroups.end())
    {
        K_LOG_ERROR( "[%s] 공통 드롭 그룹에 대한 정보가 없습니다.", commonGroup.c_str());
        return dropItems;
    }

    if(unique_it == m_uniqueGroups.end())
    {
        K_LOG_ERROR( "[%s] 유일 드롭 그룹에 대한 정보가 없습니다.", uniqueGroup.c_str());
        return dropItems;
    }
    
    if(common_it->second.minDrop > common_it->second.maxDrop)
    {
        K_LOG_ERROR( "minDrop [%d] 이 maxDrop [%d]보다 큽니다.", common_it->second.minDrop, common_it->second.maxDrop);
        return dropItems;
    }
   
    if(unique_it->second.minDrop > unique_it->second.maxDrop)
    {
        K_LOG_ERROR( "minDrop [%d] 이 maxDrop [%d]보다 큽니다.", unique_it->second.minDrop, unique_it->second.maxDrop);
        return dropItems;
    }

    // 공통 아이템 생성 
    if(!SelectDropItem(common_it, dropItems))
    {
        K_LOG_ERROR( "생성된 드롭 아이템이 없습니다.");
        return dropItems;
    }

    // 유일 아이템 생성 
    if(!SelectDropItem(unique_it, dropItems))
    {
        K_LOG_ERROR( "생성된 드롭 아이템이 없습니다.");
        return dropItems;
    }

    return dropItems;
    
}

int DropManager::CalculateWeight(std::vector<DropEntry>& entries)
{
    int sumWeight = 0;

    if(entries.empty()) return 0;

    for(auto &it : entries)
    {
        sumWeight += it.weight;
    }

    return sumWeight;
}

bool DropManager::SelectDropItem(std::unordered_map<std::string, DropGroup>::iterator Groups, std::vector<DropResult>& dropItems)
{
    DropGroup& group = Groups->second;

    if (group.minDrop < 0 || group.maxDrop < 0 || group.minDrop > group.maxDrop)
    {
        K_LOG_ERROR( "invalid drop range. minDrop=%d, maxDrop=%d", group.minDrop, group.maxDrop);
        return false;
    }

    if (group.entries.empty())
    {
        K_LOG_ERROR( "drop entries is empty.");
        return false;
    }

    std::uniform_int_distribution<int> randDropCount(group.minDrop, group.maxDrop);
    int dropCount = randDropCount(m_gen);

    std::vector<DropEntry> entries = group.entries;

    for (int i = 0; i < dropCount; ++i)
    {
        if (entries.empty())
        {
            break;
        }

        int sumWeight = CalculateWeight(entries);

        if (sumWeight <= 0)
        {
            K_LOG_ERROR( "invalid sumWeight=%d", sumWeight);
            break;
        }

        std::uniform_int_distribution<int> randWeight(1, sumWeight);
        int dropWeight = randWeight(m_gen);

        int accWeight = 0;
        bool selected = false;

        for (size_t idx = 0; idx < entries.size(); ++idx)
        {
            if (entries[idx].weight <= 0)
            {
                continue;
            }

            accWeight += entries[idx].weight;

            if (dropWeight <= accWeight)
            {
                const DropEntry& selected_item = entries[idx];

                if (selected_item.minCount < 0 ||
                    selected_item.maxCount < 0 ||
                    selected_item.minCount > selected_item.maxCount)
                {
                    K_LOG_ERROR( "invalid item count range. itemId=%d, minCount=%d, maxCount=%d", selected_item.itemId, selected_item.minCount, selected_item.maxCount);

                    return false;
                }

                std::uniform_int_distribution<int> randCount(
                    selected_item.minCount,
                    selected_item.maxCount
                );

                DropResult result;
                result.type = selected_item.type;
                K_LOG_DEBUG( "selected_item.itemId [%d]", selected_item.itemId);
                result.itemId = selected_item.itemId;
                result.count = randCount(m_gen);

                dropItems.push_back(result);

                if (!group.allowDuplicate)
                {
                    entries.erase(entries.begin() + idx);
                }

                selected = true;
                break;
            }
        }

        if (!selected)
        {
            K_LOG_ERROR( "no drop item selected. dropWeight=%d, sumWeight=%d, entries=%zu", dropWeight, sumWeight, entries.size());

            break;
        }
    }

    return true;
}
