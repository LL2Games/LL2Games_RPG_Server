#include "MySqlConnectionPool.h"

MySqlConnectionPool *MySqlConnectionPool::m_instance=nullptr;

int MySqlConnectionPool::Init(const MySqlConfig& mysqlConfig, const int pool_size)
{
    if (m_instance != nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] Already Init ", __FUNCTION__, __LINE__);
        return -1;
    }

    MySqlConnectionPool* client = new MySqlConnectionPool(mysqlConfig, pool_size == 0 ? MYSQL_POOL_SIZE : pool_size);
    if (client == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] Memory error(new MySqlConnectionPool(mysqlConfig, pool_size)) ", __FUNCTION__, __LINE__);
        return -1;
    }

    if (client->GetPoolSize() == 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] connect fail host=%s, port=%d", __FUNCTION__, __LINE__, mysqlConfig.host.c_str(), mysqlConfig.port);
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] connect fail user=%s, database=%s", __FUNCTION__, __LINE__, mysqlConfig.user.c_str(), mysqlConfig.database.c_str());
        delete client;
        return -1;
    }
    
    m_instance = client;
    return 0;
}

int MySqlConnectionPool::GetPoolSize() const
{
    return m_pool.size();
}

MySqlConnectionPool::MySqlConnectionPool(const MySqlConfig& mysqlConfig, const int pool_size)
{
    int cnt = 0;

    for (int i = 0; i < pool_size; i++)
    {
        MYSQL* conn = mysql_init(nullptr);
        if (!conn)
        {
            K_slog_trace(K_SLOG_ERROR, "[%s][%d] mysql_init ERROR", __FUNCTION__, __LINE__);
            continue;
        }

        MYSQL* result = mysql_real_connect(
            conn,
            mysqlConfig.host.c_str(),
            mysqlConfig.user.c_str(),
            mysqlConfig.password.c_str(),
            mysqlConfig.database.c_str(),
            mysqlConfig.port,
            nullptr,
            0
        );

        if (result == nullptr)
        {
            K_slog_trace(K_SLOG_ERROR, "[%s][%d] mysql_real_connect failed : %s",
                         __FUNCTION__, __LINE__, mysql_error(conn));
            mysql_close(conn);
            continue;
        }


        m_pool.push(conn);
        cnt++;
    }

    K_slog_trace(K_SLOG_TRACE, "[%s][%d] db pool created[%d]", __FUNCTION__, __LINE__, cnt);
}

MySqlConnectionPool::~MySqlConnectionPool()
{
    if(m_instance)
    {
        delete m_instance;
        m_instance = nullptr;
    }
}

MySqlConnectionPool* MySqlConnectionPool::GetInstance()
{
    if(m_instance == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] Not Initialized mysql (first call MySqlConnectionPool::Init) ", __FUNCTION__, __LINE__);
        return nullptr;
    }

    return m_instance;
}

MYSQL* MySqlConnectionPool::GetConnection()
{
    std::lock_guard<std::mutex> lock(m_sqlMutex); 
    // m_pool이 생성안됐을 때 대비해서 안전 코드 생성
    if(m_pool.empty())
    {
        return nullptr;
    }
    MYSQL* conn = m_pool.front();
    m_pool.pop();
    return conn;
}

int MySqlConnectionPool::ReleaseConnection(MYSQL* conn)
{
    if (conn == nullptr)
    {
        return -1;
    }

    std::lock_guard<std::mutex> lock(m_sqlMutex);
    m_pool.push(conn);
    return 0;
}
