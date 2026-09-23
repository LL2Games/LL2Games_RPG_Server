#include "RedisClient.h"


//RedisClient *RedisClient::m_instance =nullptr;

RedisClient::RedisClient(const RedisConfig& redisConfig) : m_ctx(nullptr)
{
    timeval connectTimeout{5, 0};

    m_ctx = redisConnectWithTimeout(redisConfig.host.c_str(), redisConfig.port, connectTimeout);

    if (m_ctx == nullptr)
    {
        K_LOG_ERROR("Redis context allocation failed");
        return;
    }

    if (m_ctx->err)
    {
        K_LOG_ERROR("Redis connect failed: %s",m_ctx->errstr);

        redisFree(m_ctx);
        m_ctx = nullptr;
        return;
    }

    timeval commandTimeout{5, 0};
    redisSetTimeout(m_ctx, commandTimeout);
    redisEnableKeepAlive(m_ctx);
}

RedisClient::~RedisClient()
{
 
    if(m_ctx)
    {
        redisFree(m_ctx);
        m_ctx = nullptr;
    }

}

bool RedisClient::IsConnected() const
{
    return m_ctx != nullptr;
}

bool RedisClient::EnsureConnected()
{
    if (m_ctx == nullptr)
    {
        K_LOG_ERROR("Redis context is null");
        return false;
    }

    auto ping = [this]() -> bool
    {
        redisReply* reply = static_cast<redisReply*>(redisCommand(m_ctx, "PING"));

        if (reply == nullptr)
            return false;

        const bool success =
            reply->type == REDIS_REPLY_STATUS &&
            reply->str != nullptr &&
            std::string(reply->str, reply->len) == "PONG";

        freeReplyObject(reply);
        return success;
    };

    // 기존 연결이 살아 있으면 그대로 사용
    if (ping())
        return true;

    K_LOG_ERROR(
        "Redis connection lost. reconnecting. error[%d] message[%s]",
        m_ctx->err,
        m_ctx->errstr);

    // 기존 context가 기억하고 있는 IP/포트로 재접속
    if (redisReconnect(m_ctx) != REDIS_OK)
    {
        K_LOG_ERROR(
            "Redis reconnect failed. error[%d] message[%s]",
            m_ctx->err,
            m_ctx->errstr);

        return false;
    }

    // 재접속 성공 여부를 PING으로 다시 확인
    if (!ping())
    {
        K_LOG_ERROR("Redis PING failed after reconnect");
        return false;
    }

    K_LOG_TRACE("Redis reconnect succeeded");
    return true;
}

//int RedisClient::Init(const RedisConfig& redisConfig)
//{
//    if (m_instance != nullptr)
//    {
//        K_LOG_ERROR( "Already Init ");
//        return -1;
//    }
//
//    RedisClient* client = new RedisClient(redisConfig);
//    if (client == nullptr)
//    {
//        K_LOG_ERROR( "Memory error(new RedisClient(redisConfig)) ");
//        return -1;
//    }
//
//    if (!client->IsConnected())
//    {
//        K_LOG_ERROR( "connect fail host=%s, port=%d", redisConfig.host.c_str(), redisConfig.port);
//        delete client;
//        return -1;
//    }
//    
//    m_instance = client;
//    return EXIT_SUCCESS;
//}


//RedisClient *RedisClient::GetInstance()
//{
//    if(m_instance == nullptr)
//    {
//        K_LOG_ERROR( "Not Initialized redis (first call RedisClient::Init) ");
//        return nullptr;
//    }
//    
//    return m_instance;
//}

int RedisClient::HSet(const std::string key, const std::string& field, const std::string& value, const int expire)
{
    int rc = EXIT_SUCCESS;

    redisReply* reply = nullptr;
    if(m_ctx == nullptr)
    {
        K_LOG_ERROR( "redis context is null");
        rc = EXIT_FAILURE;
        goto err;
    }

    //HSET
    K_LOG_DEBUG( "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    reply = (redisReply*)redisCommand(m_ctx, "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    if (reply == nullptr)
    {
        K_LOG_ERROR( "HSET command failed for key: %s", key.c_str());
        rc = EXIT_FAILURE;
        goto err;
    }
    
    //유효기간 설정
    K_LOG_DEBUG( "EXPIRE %s %d", key.c_str(), expire);
    reply = (redisReply*)redisCommand(m_ctx, "EXPIRE %s %d", key.c_str(), expire);
    if (reply == nullptr)
    {
        K_LOG_ERROR( "EXPIRE command failed for key: %s, expire: %d", key.c_str(), expire);
        rc = EXIT_FAILURE;
        goto err;
    }



err:
    if(reply)
    {
        freeReplyObject(reply);
    }
    return rc;

}

int RedisClient::HSetAll(const std::string& key, std::map<std::string, std::string> redis_map, const int expire)
{
    if (m_ctx == nullptr)
    {
        K_LOG_ERROR("redis context is null");
        return EXIT_FAILURE;
    }

    if (redis_map.empty())
    {
        K_LOG_ERROR("redis map is empty");
        return EXIT_FAILURE;
    }

    std::vector<const char*> argv;
    std::vector<size_t> argvLen;

    const std::string command = "HSET";

    argv.reserve(2 + redis_map.size() * 2);
    argvLen.reserve(2 + redis_map.size() * 2);

    auto pushArgument = [&](const std::string& argument)
    {
        argv.push_back(argument.c_str());
        argvLen.push_back(argument.size());
    };

    pushArgument(command);
    pushArgument(key);

    for (const auto& [field, fieldValue] : redis_map)
    {
        pushArgument(field);
        pushArgument(fieldValue);
    }

    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(m_ctx,static_cast<int>(argv.size()),argv.data(),argvLen.data()));

    if (reply == nullptr)
    {
        K_LOG_ERROR("Redis HSET failed: %s",m_ctx->errstr ? m_ctx->errstr : "unknown error");

        return EXIT_FAILURE;
    }

    if (reply->type == REDIS_REPLY_ERROR)
    {
        K_LOG_ERROR("Redis HSET error: %s",reply->str ? reply->str : "unknown error");

        freeReplyObject(reply);
        return EXIT_FAILURE;
    }

    freeReplyObject(reply);
    reply = nullptr;

    // expire가 0 이하라면 만료를 설정하지 않는 정책
    if (expire <= 0)
    {
        return EXIT_SUCCESS;
    }

    reply = static_cast<redisReply*>(redisCommand(m_ctx,"EXPIRE %b %d",key.data(),key.size(),expire));

    if (reply == nullptr)
    {
        K_LOG_ERROR("Redis EXPIRE failed: %s",m_ctx->errstr ? m_ctx->errstr : "unknown error");
        return EXIT_FAILURE;
    }

    if (reply->type == REDIS_REPLY_ERROR)
    {
        K_LOG_ERROR("Redis EXPIRE error: %s", reply->str ? reply->str : "unknown error");

        freeReplyObject(reply);
        return EXIT_FAILURE;
    }

    // EXPIRE 결과: 1이면 적용, 0이면 키가 존재하지 않음
    if (reply->type != REDIS_REPLY_INTEGER || reply->integer != 1)
    {
        K_LOG_ERROR(
            "Redis EXPIRE was not applied. key:%s result:%lld",
            key.c_str(),
            reply->type == REDIS_REPLY_INTEGER ? reply->integer : -1LL);

        freeReplyObject(reply);
        return EXIT_FAILURE;
    }

    freeReplyObject(reply);
    return EXIT_SUCCESS;
}

std::optional<std::map<std::string, std::string>> RedisClient::HGetAll(const std::string key)
{
    std::map<std::string, std::string> result;
    redisReply* reply = nullptr;
    if(m_ctx == nullptr)
    {
        K_LOG_ERROR( "redis context is null");
        return std::nullopt;
    }

    reply = (redisReply*)redisCommand(m_ctx, "HGETALL %s", key.c_str());
    if(reply == nullptr || reply->type != REDIS_REPLY_ARRAY)
    {
        K_LOG_ERROR( "HGETALL command failed for key: %s", key.c_str());
        if(reply)
        {
            freeReplyObject(reply);
        }
        return std::nullopt;
    }

    for(size_t i=0; i < reply->elements; i+=2)
    {
        std::string field = reply->element[i]->str;
        std::string value = reply->element[i+1]->str;
        result[field] = value;
    }

    freeReplyObject(reply);
    return result;
}

RedisSetResult RedisClient::SetIfAbsentWithTtl(const std::string& key, const std::string& value, int ttlSeconds)
{
    if (m_ctx == nullptr)
    {
        K_LOG_ERROR("SetIfAbsentWithTtl failed: Redis context is null");
        return RedisSetResult::Error;
    }

    if (key.empty() || value.empty())
    {
        K_LOG_ERROR("SetIfAbsentWithTtl failed: key or value is empty");
        return RedisSetResult::Error;
    }

    if (ttlSeconds <= 0)
    {
        K_LOG_ERROR("SetIfAbsentWithTtl failed: invalid TTL");
        return RedisSetResult::Error;
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(
            m_ctx,
            "SET %b %b EX %d NX",
            key.data(),
            key.size(),
            value.data(),
            value.size(),
            ttlSeconds
        )
    );

    if (reply == nullptr)
    {
        K_LOG_ERROR("SetIfAbsentWithTtl failed: Redis command failed");
        return RedisSetResult::Error;
    }

    RedisSetResult result = RedisSetResult::Error;

    if (reply->type == REDIS_REPLY_STATUS &&
        reply->str != nullptr &&
        std::string(reply->str, reply->len) == "OK")
    {
        result = RedisSetResult::Stored;
    }
    else if (reply->type == REDIS_REPLY_NIL)
    {
        // NX 조건에 의해 기존 키를 덮어쓰지 않음
        result = RedisSetResult::AlreadyExists;
    }
    else
    {
        K_LOG_ERROR("SetIfAbsentWithTtl failed: unexpected Redis reply");
    }

    freeReplyObject(reply);

    return result;
}

RedisGetDelResult RedisClient::GetAndDelete(const std::string& key)
{
     if (m_ctx == nullptr)
    {
        K_LOG_ERROR("GetAndDelete failed: Redis context is null");
        return {RedisGetDelStatus::Error,{}};
    }

    if (key.empty())
    {
        K_LOG_ERROR("GetAndDelete failed: key is empty");
        return {RedisGetDelStatus::Error,{}};
    }

    redisReply* reply = static_cast<redisReply*>(redisCommand(m_ctx,"GETDEL %b",key.data(),key.size()));

    if (reply == nullptr)
    {
        K_LOG_ERROR("GetAndDelete failed: Redis command failed");
        return {RedisGetDelStatus::Error,{}};
    }

    RedisGetDelResult result;

    if (reply->type == REDIS_REPLY_STRING)
    {
        result.status = RedisGetDelStatus::Found;

        if (reply->str != nullptr)
        {
            result.value.assign(reply->str,reply->len);
        }
    }
    else if (reply->type == REDIS_REPLY_NIL)
    {
        // 키가 없거나, 만료됐거나, 이미 사용된 티켓
        result.status = RedisGetDelStatus::NotFound;
    }
    else
    {
        K_LOG_ERROR("GetAndDelete failed: unexpected Redis reply type [%d]",reply->type);
        result.status = RedisGetDelStatus::Error;
    }

    freeReplyObject(reply);

    return result;
}

bool RedisClient::Delete(const std::string& key)
{
    if (m_ctx == nullptr)
    {
        K_LOG_ERROR("Delete failed: Redis context is null");
        return false;
    }

    if (key.empty())
    {
        K_LOG_ERROR("Delete failed: key is empty");
        return false;
    }

    redisReply* reply = static_cast<redisReply*>(redisCommand(m_ctx,"DEL %b",key.data(),key.size()));

    if (reply == nullptr)
    {
        K_LOG_ERROR("Delete failed: Redis command failed. key[%s]",key.c_str());
        return false;
    }

    const bool succeeded = reply->type == REDIS_REPLY_INTEGER;

    if (!succeeded)
    {
        K_LOG_ERROR("Delete failed: unexpected reply type[%d]",reply->type);
    }

    freeReplyObject(reply);

    return succeeded;
}