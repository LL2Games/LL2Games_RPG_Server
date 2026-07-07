#pragma once

#include "common.h"
#include "CommonEnum.h"
#include "MySqlConnectionPool.h"
#include "RedisClient.h"
#include <memory>


class LevelManager
{
public:
    LevelManager();
    ~LevelManager(){};

    bool Init();
    bool LoadLevelTable();
    int64_t GetNeedExp(int level) const;

    static LevelManager *GetInstance();
private:
    std::unordered_map<int, int64_t> m_needExpTable;

    static LevelManager *m_instance;

    static MySqlConnectionPool* m_mySql;
};