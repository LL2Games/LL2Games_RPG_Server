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

    static void Shutdown() noexcept;
private:
    ~MySqlConnectionPool();
    explicit MySqlConnectionPool(const MySqlConfig& mysqlConfig, const int pool_size);

    MYSQL* CreateConnection();
private:
    std::queue<MYSQL*> m_pool;
    static MySqlConnectionPool* m_instance;

    MySqlConfig m_config;

    mutable std::mutex m_sqlMutex;
    //config의 mysql.poolCount 값
    std::size_t m_targetPoolSize = 0;
    // 대기 중 + 사용 중인 실제 연결 수
    std::size_t m_liveConnectionCount = 0;
};



