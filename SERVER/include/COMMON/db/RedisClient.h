#pragma once
#include "common.h"
#include <optional>
#include "ConfigLoader.h"


enum class RedisSetResult
{
    Stored,
    AlreadyExists,
    Error,
};

enum class RedisGetDelStatus
{
    Found,
    NotFound,
    Error
};

struct RedisGetDelResult
{
    RedisGetDelStatus status = RedisGetDelStatus::Error;
    std::string value;
};


class RedisClient
{
public:
    
    ~RedisClient();

    bool IsConnected() const;
    //static int Init(const RedisConfig& redisConfig);
    //static RedisClient *GetInstance();

    int Set(const std::string key, const std::string value);
    std::string Get(std::string key);

    int HSet(const std::string key, const std::string& field, const std::string &value, const int expire =60);
    int HSetAll(const std::string& key, std::map<std::string, std::string> redis_map, const int expire);
    std::optional<std::map<std::string, std::string>> HGetAll(const std::string key);

    bool Delete(const std::string& key);
private:
    redisContext* m_ctx;
    //static RedisClient *m_instance;

public:
    explicit RedisClient(const RedisConfig& redisConfig);

    // Redis에 인증 토큰 저장
    RedisSetResult SetIfAbsentWithTtl(const std::string& key, const std::string& value, int ttlSeconds);

    // Redis에서 데이터를 가져오고 삭제하는 함수
    RedisGetDelResult GetAndDelete(const std::string& key);
};
