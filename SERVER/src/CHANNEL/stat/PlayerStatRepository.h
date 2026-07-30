#pragma once
#include "CharacterStat.h"
#include "MySqlConnectionPool.h"
#include "RedisClient.h"

int UpdateExpLevel(
    int charId,
    int level,
    int64_t exp,
    int remainAp,
    std::string& errMsg
);

class PlayerStatRepository
{
public:
    PlayerStatRepository();
    ~PlayerStatRepository();

    int Update(const std::string& charId, const std::string& statType, std::string &errMsg);
    int UpdateExpLevel(int charId, int level, int64_t exp, int remainAp, std::string& errMsg);
    int SaveRuntimeStat(int charId, const CharacterStat& stat, std::string& errMsg);
private:
    MySqlConnectionPool* m_mySql;
};