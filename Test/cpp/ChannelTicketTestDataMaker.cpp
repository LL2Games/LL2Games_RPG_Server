#include "AuthTicketService.h"
#include "RedisClient.h"
#include "config.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace
{
    bool ParsePositiveInt(const char* text, int& value)
    {
        if (text == nullptr)
        {
            return false;
        }

        try
        {
            const std::string input(text);
            std::size_t parsedLength = 0;
            const long long parsedValue = std::stoll(input, &parsedLength);

            if (parsedLength != input.size() || parsedValue <= 0 || parsedValue > std::numeric_limits<int>::max())
            {
                return false;
            }

            value = static_cast<int>(parsedValue);
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
}

int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        std::cerr << "사용법: " << argv[0] << " <시작 캐릭터 ID> <발급 개수> <Channel ID> <출력 파일>\n";

        return EXIT_FAILURE;
    }

    int startCharacterId = 0;
    int ticketCount = 0;
    int channelId = 0;

    if (!ParsePositiveInt(argv[1], startCharacterId) ||
        !ParsePositiveInt(argv[2], ticketCount) ||
        !ParsePositiveInt(argv[3], channelId))
    {
        std::cerr << "[FAIL] 실행 인자가 올바른 양의 정수가 아님\n";
        return EXIT_FAILURE;
    }

    if (ticketCount > 10000)
    {
        std::cerr << "[FAIL] 한 번에 최대 10000개까지 발급 가능\n";
        return EXIT_FAILURE;
    }

    if (startCharacterId >
        std::numeric_limits<int>::max() - ticketCount + 1)
    {
        std::cerr << "[FAIL] 캐릭터 ID 범위 초과\n";
        return EXIT_FAILURE;
    }

    const char* redisHost = std::getenv("TEST_REDIS_HOST");
    const char* redisPortText = std::getenv("TEST_REDIS_PORT");

    if (redisHost == nullptr || redisHost[0] == '\0')
    {
        std::cerr << "[FAIL] TEST_REDIS_HOST가 설정되지 않음\n";
        return EXIT_FAILURE;
    }

    int redisPort = 6379;

    if (redisPortText != nullptr && !ParsePositiveInt(redisPortText, redisPort))
    {
        std::cerr << "[FAIL] TEST_REDIS_PORT가 올바르지 않음\n";
        return EXIT_FAILURE;
    }

    if (redisPort > 65535)
    {
        std::cerr << "[FAIL] Redis 포트 범위 초과\n";
        return EXIT_FAILURE;
    }

    RedisConfig redisConfig{};
    redisConfig.host = redisHost;
    redisConfig.port = redisPort;
    redisConfig.poolCount = 1;

    RedisClient redis(redisConfig);

    if (!redis.IsConnected())
    {
        std::cerr << "[FAIL] 테스트 Redis 연결 실패\n";
        return EXIT_FAILURE;
    }

    std::ofstream outputFile(argv[4],std::ios::out | std::ios::trunc);

    if (!outputFile.is_open())
    {
        std::cerr << "[FAIL] 출력 파일 생성 실패\n";
        return EXIT_FAILURE;
    }

    for (int index = 0; index < ticketCount; ++index)
    {
        const int characterId = startCharacterId + index;

        ChannelTicketClaims claims;
        claims.accountId = "channel-load-test-" + std::to_string(characterId);
        claims.characterId = characterId;
        claims.channelId = channelId;

        const auto ticket = AuthTicketService::IssueChannelTicket(redis, claims);

        if (!ticket.has_value())
        {
            std::cerr << "[FAIL] Channel 입장권 발급 실패. characterId=" << characterId << '\n';
            return EXIT_FAILURE;
        }

        outputFile << characterId << ' ' << *ticket << '\n';
    }

    outputFile.flush();

    if (!outputFile)
    {
        std::cerr << "[FAIL] 입장권 파일 저장 실패\n";
        return EXIT_FAILURE;
    }

    std::cout << "[PASS] Channel 입장권 " << ticketCount << "개 발급 완료\n";

    std::cout << "[PASS] 출력 파일: " << argv[4] << '\n';

    return EXIT_SUCCESS;
}