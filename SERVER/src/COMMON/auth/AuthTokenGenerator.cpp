#include "AuthTokenGenerator.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <sys/random.h>

std::string AuthTokenGenerator::Generate()
{
    // 32 바이트 공간 생성
    constexpr std::size_t kTokenByteSize = 32;

    std::array<std::uint8_t, kTokenByteSize> randomBytes{};

    std::size_t receivedSize = 0;

    while(receivedSize < randomBytes.size())
    {
        // Linux에 난수 요청
        const ssize_t result = getrandom(
            randomBytes.data() + receivedSize,
            randomBytes.size() - receivedSize,
            0);
        
        //32바이트가 채워질 때까지 반복
        if(result > 0)
        {
            receivedSize += static_cast<std::size_t>(result);
            continue;
        }

        if(result < 0 && errno == EINTR)
        {
            continue;
        }

        throw std::runtime_error("getrandom failed");
    }
    // 바이너리를 16진수 문자열로 변환
    constexpr char kHexTable[] = "0123456789abcdef";


    std::string token;
    token.resize(randomBytes.size() *2);

    // 32비트를 16진수 문자열로 인코딩
    for(std::size_t i = 0; i < randomBytes.size(); ++i)
    {
        const std::uint8_t value = randomBytes[i];

        token[i*2] = kHexTable[value >> 4];
        token[i*2+1] = kHexTable[value & 0x0F];
    }

    return token;
}