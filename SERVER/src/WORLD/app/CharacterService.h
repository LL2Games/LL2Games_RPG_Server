#pragma once
#include <string>
#include <vector>
#include "MySqlConnectionPool.h"
#include "RedisClient.h"

class CharacterService
{
public:
    CharacterService();
    ~CharacterService();
    std::vector<std::string> GetCharacterList(const std::string& account_id, RedisClient& redis);
    int LoadCharacterSummary(const std::string& char_id);
    int CacheCharacterSummary(const std::string& char_id);
private:
    MySqlConnectionPool* m_db = nullptr;
    // RedisClient& m_redis;
};