#pragma once

#include <optional>
#include <string>

class RedisClient;

class AuthTicketService
{
public:
    static std::optional<std::string> IssueWorldTicket(RedisClient& redis,const std::string& accountId);
    static std::optional<std::string> ConsumeWorldTicket(RedisClient& redis, const std::string& ticket);


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