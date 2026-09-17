#include "MySqlConnectionPool.h"

MySqlConnectionPool *MySqlConnectionPool::m_instance=nullptr;

int MySqlConnectionPool::Init(const MySqlConfig& mysqlConfig, const int pool_size)
{
    if (m_instance != nullptr)
    {
        K_LOG_ERROR( "Already Init ");
        return -1;
    }

     if (pool_size <= 0)
    {
        K_LOG_ERROR("Invalid MySQL pool size configured: %d",pool_size);

        return EXIT_FAILURE;
    }

    MySqlConnectionPool* client = new MySqlConnectionPool(mysqlConfig, pool_size);
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
    std::lock_guard<std::mutex> lock(m_sqlMutex);

    return static_cast<int>(m_liveConnectionCount);
}


MySqlConnectionPool::MySqlConnectionPool(const MySqlConfig& mysqlConfig, const int pool_size) : m_config(mysqlConfig), m_targetPoolSize(static_cast<std::size_t>(pool_size))
{
    for (int i = 0; i < pool_size; ++i)
    {
        MYSQL* conn = CreateConnection();

        if (conn == nullptr)
            continue;

        m_pool.push(conn);
        ++m_liveConnectionCount;
    }

    K_LOG_TRACE("db pool created[%zu/%zu]", m_liveConnectionCount,m_targetPoolSize);
}

MySqlConnectionPool::~MySqlConnectionPool()
{
    std::lock_guard<std::mutex> lock(m_sqlMutex);

    while (!m_pool.empty())
    {
        MYSQL* conn = m_pool.front();
        m_pool.pop();

        if (conn != nullptr)
            mysql_close(conn);
    }

    m_liveConnectionCount = 0;
}

void MySqlConnectionPool::Shutdown() noexcept
{
    MySqlConnectionPool* instance = m_instance;

    // delete 전에 nullptr로 변경해야 재진입 위험이 없다.
    m_instance = nullptr;

    delete instance;
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
    bool shouldCreate = false;

    {
        std::lock_guard<std::mutex> lock(m_sqlMutex);

        if (!m_pool.empty())
        {
            conn = m_pool.front();
            m_pool.pop();
        }
        else if(m_liveConnectionCount < m_targetPoolSize)
        {
            // 유실된 연결 슬롯을 예약한다.
            ++m_liveConnectionCount;
            shouldCreate = true;
        }
        else
        {
            // 연결이 유실된 게 아니라 전부 사용 중인 상태
            K_LOG_ERROR("MySQL connection pool exhausted");
            return nullptr;
        }
    }

    if (shouldCreate)
    {
        conn = CreateConnection();

        if (conn != nullptr)
        {
            K_LOG_TRACE("MySQL connection pool replenished");
            return conn;
        }

        {
            std::lock_guard<std::mutex> lock(m_sqlMutex);

            if (m_liveConnectionCount > 0)
                --m_liveConnectionCount;
        }

        K_LOG_ERROR("Failed to replenish MySQL connection pool");
        return nullptr;
    }

    if (mysql_ping(conn) == 0)
        return conn;

    K_LOG_ERROR(
        "Dead MySQL connection detected: %s",
        mysql_error(conn)
    );

    mysql_close(conn);

    // 기존 연결의 자리를 즉시 교체한다.
    conn = CreateConnection();

    if (conn != nullptr)
    {
        K_LOG_TRACE("Dead MySQL connection replaced");
        return conn;
    }

    // 교체 실패로 실제 연결 하나가 유실됐다.
    {
        std::lock_guard<std::mutex> lock(m_sqlMutex);

        if (m_liveConnectionCount > 0)
            --m_liveConnectionCount;
    }

    K_LOG_ERROR(
        "Failed to replace dead MySQL connection; "
        "a later request will retry"
    );

    return nullptr;
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
