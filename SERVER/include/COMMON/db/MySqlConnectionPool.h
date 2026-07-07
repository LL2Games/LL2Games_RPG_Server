#pragma once
#include "common.h"
#include <queue>
#include <mutex>
#include "ConfigLoader.h"
#define MYSQL_POOL_SIZE 8
class MySqlConnectionPool
{
public:
    static MySqlConnectionPool *GetInstance();
    static int Init(const MySqlConfig& mysqlConfig, const int pool_size = MYSQL_POOL_SIZE);
    MYSQL* GetConnection();
    int ReleaseConnection(MYSQL*);
    int GetPoolSize() const;
private:
    explicit MySqlConnectionPool(const MySqlConfig& mysqlConfig, const int pool_size);
    std::queue<MYSQL*> m_pool;
    ~MySqlConnectionPool();
    static MySqlConnectionPool* m_instance;
    std::mutex m_sqlMutex;
};



