#include "AuthTokenGenerator.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <unordered_set>

int main()
{
    constexpr int kTestCount = 10000;
    constexpr std::size_t kExpectedTokenLength = 64;

    std::unordered_set<std::string> generatedTokens;
    generatedTokens.reserve(kTestCount);

    try
    {
        for (int i = 0; i < kTestCount; ++i)
        {
            const std::string token =
                AuthTokenGenerator::Generate();

            if (token.size() != kExpectedTokenLength)
            {
                std::cerr
                    << "[FAIL] invalid token length: "
                    << token.size()
                    << '\n';

                return EXIT_FAILURE;
            }

            const bool isLowercaseHex = std::all_of(
                token.begin(),
                token.end(),
                [](char value)
                {
                    return (value >= '0' && value <= '9') ||
                           (value >= 'a' && value <= 'f');
                }
            );

            if (!isLowercaseHex)
            {
                std::cerr
                    << "[FAIL] token contains non-hex character\n";

                return EXIT_FAILURE;
            }

            if (!generatedTokens.insert(token).second)
            {
                std::cerr
                    << "[FAIL] duplicate token generated\n";

                return EXIT_FAILURE;
            }
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "[FAIL] token generation exception: "
            << exception.what()
            << '\n';

        return EXIT_FAILURE;
    }

    std::cout << "[PASS] token length is 64 characters\n";
    std::cout << "[PASS] token contains lowercase hex characters only\n";
    std::cout << "[PASS] generated "
              << kTestCount
              << " unique tokens\n";
    std::cout << "All auth token generator tests passed\n";

    return EXIT_SUCCESS;
}