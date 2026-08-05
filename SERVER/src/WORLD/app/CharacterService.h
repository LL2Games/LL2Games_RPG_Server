#pragma once
#include <string>
#include <vector>
#include "MySqlConnectionPool.h"
#include "RedisClient.h"

struct CreateCharacterResult
{
    bool success = false;
    long long char_id = 0;
    std::string error;
};


class CharacterService
{
public:
    CharacterService();
    ~CharacterService();
    int CreateCharacter(const std::string& account_id, const std::string& name, int job);
    std::vector<std::string> GetCharacterList(const std::string& account_id, RedisClient& redis);
    int LoadCharacterSummary(const std::string& char_id);
    int CacheCharacterSummary(const std::string& char_id);
    int CheckDupNick(const std::string& nick);
private:
    MySqlConnectionPool* m_db = nullptr;
    // RedisClient& m_redis;
};