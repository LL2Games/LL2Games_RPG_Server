#include "AuthTicketService.h"

#include "AuthTokenGenerator.h"
#include "PacketParser.h"
#include "RedisClient.h"
#include "K_slog.h"

#include <exception>
#include <optional>
#include <string>
#include <algorithm>

namespace
{
    constexpr int kWorldTicketTtlSeconds = 300;
    constexpr int kChannelTicketTtlSeconds = 60;
    constexpr int kMaxTicketIssueAttempts = 3;
    constexpr std::size_t kTicketLength = 64;

    const std::string kWorldTicketKeyPrefix ="auth:world:";
    constexpr char kChannelTicketKeyPrefix[] = "auth:channel:";

    bool IsValidTicketFormat(const std::string& ticket)
    {
        if(ticket.size() != kTicketLength)
        {
            return false;
        }

        return std::all_of(ticket.begin(), ticket.end(),[](char value)
        {
            return (value >= '0' && value <= '9') || (value >= 'a' && value <='f');
        });
    }
}

std::optional<std::string>AuthTicketService::IssueWorldTicket(
    RedisClient& redis,
    const std::string& accountId)
{
    if (accountId.empty())
    {
        K_LOG_ERROR("IssueWorldTicket failed: account ID is empty");
        return std::nullopt;
    }

    return IssueTicket(redis, kWorldTicketKeyPrefix,accountId,kWorldTicketTtlSeconds);
}

std::optional<std::string>AuthTicketService::IssueChannelTicket(
    RedisClient& redis,
    const ChannelTicketClaims& claims
)
{
    if (claims.accountId.empty() || claims.characterId <= 0 ||claims.channelId <= 0)
    {
        K_LOG_ERROR("IssueChannelTicket failed: invalid claims");
        return std::nullopt;
    }

    try
    {
        const std::string serializedClaims = PacketParser::MakeBody({claims.accountId, std::to_string(claims.characterId),std::to_string(claims.channelId)});

        return IssueTicket(redis, kChannelTicketKeyPrefix, serializedClaims, kChannelTicketTtlSeconds);
    }
    catch (const std::exception& exception)
    {
        K_LOG_ERROR(
            "IssueChannelTicket failed: claims serialization error [%s]",
            exception.what()
        );

        return std::nullopt;
    }
}

std::optional<std::string> AuthTicketService::ConsumeWorldTicket(RedisClient& redis, const std::string& ticket)
{
    return ConsumeTicket(redis, kWorldTicketKeyPrefix, ticket);
}

std::optional<ChannelTicketClaims> AuthTicketService::ConsumeChannelTicket(RedisClient& redis, const std::string& ticket)
{
    const auto serializedClaims = ConsumeTicket(redis,kChannelTicketKeyPrefix,ticket);

    if (!serializedClaims.has_value())
    {
        return std::nullopt;
    }

    ChannelTicketClaims claims;
    std::size_t offset = 0;
    std::string parseError;

    if (!PacketParser::ParseLengthPrefixedString(
            serializedClaims->data(),
            serializedClaims->size(),
            offset,
            claims.accountId,
            parseError))
    {
        K_LOG_ERROR("ConsumeChannelTicket failed: account ID parse error [%s]",parseError.c_str());
        return std::nullopt;
    }

    if (!PacketParser::ParseNextIntField(
            serializedClaims->data(),
            serializedClaims->size(),
            offset,
            claims.characterId,
            parseError))
    {
        K_LOG_ERROR("ConsumeChannelTicket failed: character ID parse error [%s]",parseError.c_str());
        return std::nullopt;
    }

    if (!PacketParser::ParseNextIntField(
            serializedClaims->data(),
            serializedClaims->size(),
            offset,
            claims.channelId,
            parseError))
    {
        K_LOG_ERROR("ConsumeChannelTicket failed: channel ID parse error [%s]",parseError.c_str());

        return std::nullopt;
    }

    if (offset != serializedClaims->size())
    {
        K_LOG_ERROR("ConsumeChannelTicket failed: trailing claims data");
        return std::nullopt;
    }

    if (claims.accountId.empty() || claims.characterId <= 0 || claims.channelId <= 0)
    {
        K_LOG_ERROR("ConsumeChannelTicket failed: invalid claims");
        return std::nullopt;
    }

    return claims;
}


std::optional<std::string>AuthTicketService::IssueTicket(
    RedisClient& redis,
    const std::string& keyPrefix,
    const std::string& value,
    int ttlSeconds)
{
    if (keyPrefix.empty() ||value.empty() || ttlSeconds <= 0)
    {
        K_LOG_ERROR("IssueTicket failed: invalid argument");
        return std::nullopt;
    }

    for (int attempt = 0; attempt < kMaxTicketIssueAttempts; ++attempt)
    {
        std::string ticket;

        try
        {
            ticket = AuthTokenGenerator::Generate();
        }
        catch (const std::exception& exception)
        {
            K_LOG_ERROR("IssueTicket failed: token generation error [%s]",exception.what());
            return std::nullopt;
        }
        // redis key 설정
        const std::string redisKey = keyPrefix + ticket;
        // redis에 저장
        const RedisSetResult setResult = redis.SetIfAbsentWithTtl(redisKey, value, ttlSeconds);

        if (setResult == RedisSetResult::Stored)
        {
            return ticket;
        }

        if (setResult == RedisSetResult::Error)
        {
            K_LOG_ERROR("IssueTicket failed: Redis storage error");
            return std::nullopt;
        }

        // AlreadyExists이면 충돌로 판단하고
        // 새로운 난수 티켓을 생성해 다시 시도한다.
    }

    K_LOG_ERROR("IssueTicket failed: ticket collision limit exceeded");

    return std::nullopt;
}

std::optional<std::string> AuthTicketService::ConsumeTicket(RedisClient& redis,const std::string& keyPrefix,const std::string& ticket)
{
    if (keyPrefix.empty() || !IsValidTicketFormat(ticket))
    {
        K_LOG_ERROR("ConsumeTicket failed: invalid ticket format");
        return std::nullopt;
    }

    const std::string redisKey = keyPrefix + ticket;

    const RedisGetDelResult getResult = redis.GetAndDelete(redisKey);

    if (getResult.status == RedisGetDelStatus::Found)
    {
        if (getResult.value.empty())
        {
            K_LOG_ERROR("ConsumeTicket failed: stored value is empty");
            return std::nullopt;
        }

        return getResult.value;
    }

    if (getResult.status == RedisGetDelStatus::NotFound)
    {
        // 만료됐거나 이미 사용된 티켓
        return std::nullopt;
    }

    K_LOG_ERROR("ConsumeTicket failed: Redis error");
    return std::nullopt;
}