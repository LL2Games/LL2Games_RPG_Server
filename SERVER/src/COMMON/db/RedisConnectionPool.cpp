#include "RedisConnectionPool.h"
#include "K_slog.h"

bool RedisConnectionPool::Init(const RedisConfig& redisConfig, std::size_t count)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_connections.empty())
    {
        return true;
    }

    if (count == 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[RedisConnectionPool] init failed. count is zero");
        return false;
    }

    m_connections.reserve(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        auto conn = std::make_unique<RedisClient>(redisConfig);
        if (!conn->IsConnected())
        {
            K_slog_trace(K_SLOG_ERROR, "[RedisConnectionPool] redis connect failed. index:%zu", i);

            m_connections.clear();
            while (!m_available.empty())
            {
                m_available.pop();
            }

            return false;
        }

        m_available.push(conn.get());
        m_connections.push_back(std::move(conn));
    }

    K_slog_trace(K_SLOG_TRACE, "[RedisConnectionPool] init success. count:%zu", count);
    return true;
}

RedisClient* RedisConnectionPool::Acquire()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_cv.wait(lock, [this]() {
        return !m_available.empty();
    });

    RedisClient* conn = m_available.front();
    m_available.pop();

    return conn;
}

void RedisConnectionPool::Release(RedisClient* conn)
{

    if (conn == nullptr)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_available.push(conn);
    }

    m_cv.notify_one();
}

std::size_t RedisConnectionPool::Size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connections.size();
}

std::size_t RedisConnectionPool::Available() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_available.size();
}

RedisConnectionGuard::RedisConnectionGuard(RedisConnectionPool* pool)
    : m_pool(pool)
{
    if (m_pool != nullptr)
    {
        m_conn = m_pool->Acquire();
    }
}

RedisConnectionGuard::~RedisConnectionGuard()
{
    if (m_pool != nullptr && m_conn != nullptr)
    {
        m_pool->Release(m_conn);
    }
}

RedisConnectionGuard::RedisConnectionGuard(RedisConnectionGuard&& other) noexcept
    : m_pool(other.m_pool),
      m_conn(other.m_conn)
{
    other.m_pool = nullptr;
    other.m_conn = nullptr;
}

RedisConnectionGuard& RedisConnectionGuard::operator=(RedisConnectionGuard&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (m_pool != nullptr && m_conn != nullptr)
    {
        m_pool->Release(m_conn);
    }

    m_pool = other.m_pool;
    m_conn = other.m_conn;

    other.m_pool = nullptr;
    other.m_conn = nullptr;

    return *this;
}

RedisClient* RedisConnectionGuard::Get() const
{
    return m_conn;
}

RedisClient* RedisConnectionGuard::operator->() const
{
    return m_conn;
}

RedisConnectionGuard::operator bool() const
{
    return m_conn != nullptr;
}