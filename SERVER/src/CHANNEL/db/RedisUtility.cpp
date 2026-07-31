#include "RedisUtility.h"
#include "PlayerData.h"
#include <sstream>

bool GetStr(const Map& map, const std::string& key, std::string& value)
{
    auto it = map.find(key);
    if(it == map.end()) return false;
    value = it->second;
    return true;
}

bool GetInt(const Map& map, const std::string& key, int& value)
{
    auto it = map.find(key);
    if(it == map.end()) return false;
    try{
        value = std::stoi(it->second);
    }catch(...){
        return false;
    }
    return true; 
}

bool GetFloat(const Map& map, const std::string& key, float& value)
{
    auto it = map.find(key);
    if(it == map.end()) return false;
    try{
        value = std::stof(it->second);
    }catch(...){
        return false;
    }

    return true;
}

bool GetInt64(const Map& map, const std::string& key, int64_t& value)
{
    auto it = map.find(key);
    if(it == map.end()) return false;
    try{
        value = std::stof(it->second);
    }catch(...){
        return false;
    }

    return true;
}

std::map<std::string, std::string> PlayerInfoToRedisMap(const PlayerInitData playerData, const CharacterStat stat)
{
    std::map<std::string, std::string> redisMap;
    
    redisMap["char_id"] = std::to_string(playerData.char_id);
    redisMap["account_id"] = playerData.account_id;
    redisMap["name"] = playerData.name;
    redisMap["level"] = std::to_string(playerData.level);
    redisMap["job"] = std::to_string(playerData.job);
    redisMap["root_job"] = std::to_string(playerData.root_job);
    redisMap["map_id"] = std::to_string(playerData.map_id);
    redisMap["xPos"] = std::to_string(playerData.xPos);
    redisMap["yPos"] = std::to_string(playerData.yPos);

    // CharacterStat 정보 추가
    const BaseStat& base = stat.GetBase();
    const DerivedStat& derived = stat.GetDerived();
    redisMap["base_str"] = std::to_string(base.str);
    redisMap["base_dex"] = std::to_string(base.dex);
    redisMap["base_intel"] = std::to_string(base.intel);
    redisMap["base_luck"] = std::to_string(base.luck);
    redisMap["derived_maxHp"] = std::to_string(derived.maxHp);
    redisMap["derived_maxMp"] = std::to_string(derived.maxMp);
    redisMap["curHp"] = std::to_string(stat.GetCurHp());
    redisMap["curMp"] = std::to_string(stat.GetCurMp());
    redisMap["remainAp"] = std::to_string(stat.GetRemainAp());
    redisMap["level"] = std::to_string(stat.GetLevel());
    redisMap["exp"] = std::to_string(stat.GetExp());
    redisMap["need_exp"] = std::to_string(stat.GetNeedExp());
    
    return redisMap;
}

