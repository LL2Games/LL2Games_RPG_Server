#include "MySqlConnectionPool.h"

MySqlConnectionPool *MySqlConnectionPool::m_instance=nullptr;

int MySqlConnectionPool::Init(const MySqlConfig& mysqlConfig, const int pool_size)
{
    if (m_instance != nullptr)
    {
        K_LOG_ERROR( "Already Init ");
        return -1;
    }

    MySqlConnectionPool* client = new MySqlConnectionPool(mysqlConfig, pool_size == 0 ? MYSQL_POOL_SIZE : pool_size);
    if (client == nullptr)
    {
        K_LOG_ERROR( "Memory error(new MySqlConnectionPool(mysqlConfig, pool_size)) ");
        return -1;
    }

    if (client->GetPoolSize() == 0)
    {
        K_LOG_ERROR( "connect fail host=%s, port=%d", mysqlConfig.host.c_str(), mysqlConfig.port);
        K_LOG_ERROR( "connect fail user=%s, database=%s", mysqlConfig.user.c_str(), mysqlConfig.database.c_str());
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
            K_LOG_ERROR( "mysql_init ERROR");
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
            K_LOG_ERROR( "mysql_real_connect failed : %s", mysql_error(conn));
            mysql_close(conn);
            continue;
        }

        if (mysql_set_character_set(conn, "utf8mb4") != 0)
        {
            K_LOG_ERROR(
                "mysql_set_character_set failed: %s",
                mysql_error(conn));
            
            mysql_close(conn);
            continue;
        }


        m_pool.push(conn);
        cnt++;
    }

    K_LOG_TRACE( "db pool created[%d]", cnt);
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
        K_LOG_ERROR( "Not Initialized mysql (first call MySqlConnectionPool::Init) ");
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
