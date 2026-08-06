#include "AuthTicketService.h"
#include "AuthTokenGenerator.h"
#include "RedisClient.h"
#include "config.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <exception>

namespace
{
    bool Check(const bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "[FAIL] " << message << '\n';
            return false;
        }

        std::cout << "[PASS] " << message << '\n';
        return true;
    }
}

int main()
{
    const char* redisHost = std::getenv("TEST_REDIS_HOST");
    const char* redisPortText = std::getenv("TEST_REDIS_PORT");

    if (redisHost == nullptr || std::string(redisHost).empty())
    {
        std::cerr << "[FAIL] TEST_REDIS_HOST is not configured\n";
        return EXIT_FAILURE;
    }

    int redisPort = 6379;

    if (redisPortText != nullptr)
    {
        try
        {
            redisPort = std::stoi(redisPortText);
        }
        catch (const std::exception&)
        {
            std::cerr << "[FAIL] TEST_REDIS_PORT is invalid\n";
            return EXIT_FAILURE;
        }
    }

    if (redisPort <= 0 || redisPort > 65535)
    {
        std::cerr << "[FAIL] Redis port is out of range\n";
        return EXIT_FAILURE;
    }

    RedisConfig redisConfig{};
    redisConfig.host = redisHost;
    redisConfig.port = redisPort;
    redisConfig.poolCount = 1;

    RedisClient redis(redisConfig);

    if (!Check(redis.IsConnected(),"테스트 Redis 연결 성공"))
    {
        return EXIT_FAILURE;
    }

    const ChannelTicketClaims expectedClaims{"channel-ticket-replay-test",987654,1};

    const auto issuedTicket = AuthTicketService::IssueChannelTicket(redis,expectedClaims);

    if (!Check(issuedTicket.has_value(),"Channel 입장권 발급 성공"))
    {
        return EXIT_FAILURE;
    }

    const auto firstConsume = AuthTicketService::ConsumeChannelTicket(redis,*issuedTicket);

    if (!Check(firstConsume.has_value(),"최초 입장권 사용 성공"))
    {
        return EXIT_FAILURE;
    }

    if (!Check(firstConsume->accountId == expectedClaims.accountId &&
                firstConsume->characterId == expectedClaims.characterId &&
                firstConsume->channelId == expectedClaims.channelId,
                "입장권 클레임 유지 확인"))
    {
        return EXIT_FAILURE;
    }

    // 올바른 Channel ID
    if (!Check(firstConsume->MatchesChannel(expectedClaims.channelId),"일치하는 Channel ID 승인"))
    {
        return EXIT_FAILURE;
    }

    // 다른 Channel ID
    if (!Check(!firstConsume->MatchesChannel(expectedClaims.channelId + 1),"불일치 Channel ID 거부"))
    {
        return EXIT_FAILURE;
    }


    const auto secondConsume = AuthTicketService::ConsumeChannelTicket(redis,*issuedTicket);

    if (!Check(!secondConsume.has_value(),"재사용된 입장권 거부"))
    {
        return EXIT_FAILURE;
    }

    const std::string ttlTestKey ="auth:test:ttl:" +AuthTokenGenerator::Generate();

    const RedisSetResult ttlSetResult =redis.SetIfAbsentWithTtl(ttlTestKey,"ttl-test-value",1);

    if (!Check(ttlSetResult == RedisSetResult::Stored,"짧은 TTL 입장권 저장 성공"))
    {
        return EXIT_FAILURE;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    const RedisGetDelResult expiredResult = redis.GetAndDelete(ttlTestKey);

    if (!Check(expiredResult.status == RedisGetDelStatus::NotFound,"만료된 입장권 거부"))
    {
        return EXIT_FAILURE;
    }

    std::cout << "Channel 입장권 통합 테스트 전체 통과\n";

    return EXIT_SUCCESS;
}