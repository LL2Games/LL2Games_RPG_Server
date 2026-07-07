#pragma once

#include "RedisClient.h"

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

class RedisConnectionPool
{
public:
    RedisConnectionPool() = default;
    ~RedisConnectionPool() = default;

    RedisConnectionPool(const RedisConnectionPool&) = delete;
    RedisConnectionPool& operator=(const RedisConnectionPool&) = delete;

    bool Init(const RedisConfig& redisConfig, std::size_t count);

    RedisClient* Acquire();
    void Release(RedisClient* conn);

    std::size_t Size() const;
    std::size_t Available() const;

private:
    std::vector<std::unique_ptr<RedisClient>> m_connections;
    std::queue<RedisClient*> m_available;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
};

class RedisConnectionGuard
{
public:
    explicit RedisConnectionGuard(RedisConnectionPool* pool);
    ~RedisConnectionGuard();

    RedisConnectionGuard(const RedisConnectionGuard&) = delete;
    RedisConnectionGuard& operator=(const RedisConnectionGuard&) = delete;

    RedisConnectionGuard(RedisConnectionGuard&& other) noexcept;
    RedisConnectionGuard& operator=(RedisConnectionGuard&& other) noexcept;

    RedisClient* Get() const;
    RedisClient* operator->() const;
    explicit operator bool() const;

private:
    RedisConnectionPool* m_pool = nullptr;
    RedisClient* m_conn = nullptr;
};