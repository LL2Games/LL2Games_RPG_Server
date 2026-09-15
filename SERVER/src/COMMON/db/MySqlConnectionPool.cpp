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


MySqlConnectionPool::MySqlConnectionPool(const MySqlConfig& mysqlConfig, const int pool_size) : m_config(mysqlConfig)
{
    int connectedCount = 0;

    for (int i = 0; i < pool_size; ++i)
    {
        MYSQL* conn = CreateConnection();

        if (conn == nullptr)
            continue;

        m_pool.push(conn);
        ++connectedCount;
    }

    K_LOG_TRACE("db pool created[%d]", connectedCount);
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

MYSQL* MySqlConnectionPool::CreateConnection()
{
    MYSQL* conn = mysql_init(nullptr);

    if (conn == nullptr)
        return nullptr;

    unsigned int timeout = 5;
    mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(conn, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, &timeout);

    if (mysql_real_connect(
            conn,
            m_config.host.c_str(),
            m_config.user.c_str(),
            m_config.password.c_str(),
            m_config.database.c_str(),
            m_config.port,
            nullptr,
            0) == nullptr)
    {
        K_LOG_ERROR("mysql reconnect failed: %s", mysql_error(conn));
        mysql_close(conn);
        return nullptr;
    }

    if (mysql_set_character_set(conn, "utf8mb4") != 0)
    {
        mysql_close(conn);
        return nullptr;
    }

    return conn;
}

MYSQL* MySqlConnectionPool::GetConnection()
{
    MYSQL* conn = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_sqlMutex);

        if (m_pool.empty())
        {
            K_LOG_ERROR("MySQL connection pool is empty");
            return nullptr;
        }

        conn = m_pool.front();
        m_pool.pop();
    }

    if (mysql_ping(conn) == 0)
        return conn;

    K_LOG_ERROR("Dead MySQL connection detected: %s",mysql_error(conn));

    mysql_close(conn);

    conn = CreateConnection();

    if (conn == nullptr)
    {
        K_LOG_ERROR("Failed to replace dead MySQL connection");
        return nullptr;
    }

    K_LOG_TRACE("Dead MySQL connection replaced");

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
