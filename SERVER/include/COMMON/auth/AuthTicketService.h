#pragma once

#include <optional>
#include <string>

class RedisClient;

struct ChannelTicketClaims
{
    std::string accountId;
    int characterId = 0;
    int channelId = 0;

    bool MatchesChannel(const int expectedChannelId) const noexcept
    {
        return expectedChannelId > 0 && channelId == expectedChannelId;
    }
};
class AuthTicketService
{
public:
    static std::optional<std::string> IssueWorldTicket(RedisClient& redis,const std::string& accountId);
    static std::optional<std::string> ConsumeWorldTicket(RedisClient& redis, const std::string& ticket);

    static std::optional<std::string> IssueChannelTicket(RedisClient& redis,  const ChannelTicketClaims& claims);
    static std::optional<ChannelTicketClaims> ConsumeChannelTicket(RedisClient& redis, const std::string& ticket);
private:
    static std::optional<std::string> IssueTicket(
        RedisClient& redis,
        const std::string& keyPrefix,
        const std::string& value,
        int ttlSeconds
    );

    static std::optional<std::string> ConsumeTicket(
        RedisClient& redis,
        const std::string& keyPrefix,
        const std::string& ticket
    );
};