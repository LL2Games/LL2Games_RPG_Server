#pragma once
#include "CharacterStat.h"
#include "MySqlConnectionPool.h"
#include "RedisClient.h"

class PlayerStatRepository
{
public:
    PlayerStatRepository();
    ~PlayerStatRepository();

    int Update(const std::string& charId, const std::string& statType, std::string &errMsg);

private:
    MySqlConnectionPool* m_mySql;
};