#include "RedisClient.h"


//RedisClient *RedisClient::m_instance =nullptr;

RedisClient::RedisClient(const RedisConfig& redisConfig) : m_ctx(nullptr)
{
    m_ctx = redisConnect(redisConfig.host.c_str(), redisConfig.port);

    if (m_ctx == nullptr) {
        K_LOG_ERROR( "Redis Connect error: ctx is null");
        return;
    }

    if (m_ctx->err) {
        K_LOG_ERROR( "Redis Connect error(%d): %s", m_ctx->err, m_ctx->errstr);
        redisFree(m_ctx);
        m_ctx = nullptr;
    }
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
    int rc = EXIT_SUCCESS;
    std::vector<const char*> value;
    std::vector<size_t> valueLen;
    redisReply* reply = nullptr;
    std::string cmd = "HSET";

    auto push = [&](const std::string& s)
    {
        value.push_back(s.c_str());
        valueLen.push_back(s.size());
    };

    if(m_ctx == nullptr)
    {
        K_LOG_ERROR( "redis context is null");
        rc = EXIT_FAILURE;
    
    }

    if(redis_map.empty())
    {
        K_LOG_ERROR( "redis context is null");
        rc = EXIT_FAILURE;
        goto err;
    }

    value.reserve(2 + redis_map.size() *2);
    valueLen.reserve(2 + redis_map.size() *2);

   


    push(cmd);
    push(key);

    for(const auto& [field, value] : redis_map)
    {
        push(field);
        push(value);
    }

    reply = (redisReply*)redisCommandArgv(m_ctx, (int)value.size(), value.data(), valueLen.data());

    if(!reply)
    {
        K_LOG_ERROR( "redisCommandArgv is failed");
        rc = EXIT_FAILURE;
        goto err;
    }

    freeReplyObject(reply);
    reply = nullptr;

    reply = (redisReply*)redisCommand(m_ctx, "EXPIRE %s %d", key.c_str(), expire);
     if(!reply)
    {
        K_LOG_ERROR( "redisCommand EXPIIRE is failed");
        rc = EXIT_FAILURE;
        goto err;
    }

    return EXIT_SUCCESS;
err:
    if(reply)
    {
        freeReplyObject(reply);
    }
    return rc;

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