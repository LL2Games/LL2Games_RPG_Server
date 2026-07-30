#include "PacketParser.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <limits>

namespace
{
    
void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template <typename Func>
void RequireLengthError(Func&& func, const char* message)
{
    bool thrown = false;

    try
    {
        func();
    }
    catch (const std::length_error&)
    {
        thrown = true;
    }

    Require(thrown, message);
}


void TestThreeHundredByteRoundTrip()
{
    const std::string expected(300, 'A');
    const std::string body = PacketParser::MakeBody({expected});

    Require(
        body.size() == sizeof(uint16_t) + expected.size(),
        "300-byte field has an invalid encoded size"
    );

    uint16_t encodedLength = 0;
    std::memcpy(&encodedLength, body.data(), sizeof(encodedLength));
    Require(encodedLength == expected.size(), "length prefix is not 300");

    size_t offset = 0;
    std::string parsed;
    std::string error;

    const bool success = PacketParser::ParseLengthPrefixedString(
        body.data(),
        body.size(),
        offset,
        parsed,
        error
    );

    Require(success, "300-byte field could not be parsed");
    Require(error.empty(), "successful parse returned an error");
    Require(parsed == expected, "parsed field differs from its source");
    Require(offset == body.size(), "parser did not consume the full field");
}

void TestEmptyFieldRoundTrip()
{
    const std::string body = PacketParser::MakeBody({""});
    size_t offset = 0;
    std::string parsed;
    std::string error;

    const bool success = PacketParser::ParseLengthPrefixedString(
        body.data(),
        body.size(),
        offset,
        parsed,
        error
    );

    Require(success, "empty field could not be parsed");
    Require(parsed.empty(), "empty field produced non-empty data");
    Require(offset == body.size(), "empty field was not fully consumed");
}

void TestTruncatedLengthHeader()
{
    const std::string body(1, '\0');
    size_t offset = 0;
    std::string parsed;
    std::string error;

    const bool success = PacketParser::ParseLengthPrefixedString(
        body.data(),
        body.size(),
        offset,
        parsed,
        error
    );

    Require(!success, "one-byte length header was accepted");
    Require(!error.empty(), "truncated header did not return an error");
}

void TestDeclaredLengthOverflow()
{
    const uint16_t declaredLength = 300;
    std::string body;
    body.append(
        reinterpret_cast<const char*>(&declaredLength),
        sizeof(declaredLength)
    );
    body.append(10, 'A');

    size_t offset = 0;
    std::string parsed;
    std::string error;

    const bool success = PacketParser::ParseLengthPrefixedString(
        body.data(),
        body.size(),
        offset,
        parsed,
        error
    );

    Require(!success, "short payload was accepted");
    Require(!error.empty(), "short payload did not return an error");
}

void TestMakePacketRoundTrip()
{
    constexpr uint16_t expectedType = 123;
    const std::string body = "test-body";

    const std::string packet =
        PacketParser::MakePacket(expectedType, body);

    Require(
        packet.size() == sizeof(PacketHeader) + body.size(),
        "packet size is incorrect"
    );

    PacketHeader header{};
    std::memcpy(
        &header,
        packet.data(),
        sizeof(header)
    );

    Require(
        header.type == expectedType,
        "packet type is incorrect"
    );

    Require(
        header.length == packet.size(),
        "packet header length is incorrect"
    );

    const std::string parsedBody(
        packet.data() + sizeof(PacketHeader),
        body.size()
    );

    Require(
        parsedBody == body,
        "packet body is incorrect"
    );
}

void TestMaximumPacketSizeAccepted()
{
    const size_t maximumPacketSize = std::min(
        static_cast<size_t>(BUFFER_SIZE),
        static_cast<size_t>(
            std::numeric_limits<uint16_t>::max()
        )
    );

    Require(
        maximumPacketSize >= sizeof(PacketHeader),
        "maximum packet size is smaller than header"
    );

    const size_t maximumBodySize =
        maximumPacketSize - sizeof(PacketHeader);

    const std::string body(maximumBodySize, 'A');

    const std::string packet =
        PacketParser::MakePacket(123, body);

    Require(
        packet.size() == maximumPacketSize,
        "maximum-size packet has an incorrect size"
    );

    PacketHeader header{};
    std::memcpy(
        &header,
        packet.data(),
        sizeof(header)
    );

    Require(
        header.length == maximumPacketSize,
        "maximum-size packet length is incorrect"
    );
}

void TestPacketSizeOverflowRejected()
{
    const size_t maximumPacketSize = std::min(
        static_cast<size_t>(BUFFER_SIZE),
        static_cast<size_t>(
            std::numeric_limits<uint16_t>::max()
        )
    );

    const size_t overflowBodySize =
        maximumPacketSize - sizeof(PacketHeader) + 1;

    const std::string body(overflowBodySize, 'A');

    RequireLengthError(
        [&]()
        {
            PacketParser::MakePacket(123, body);
        },
        "oversized packet was accepted"
    );
}

void TestUint16PacketLengthOverflowRejected()
{
    const size_t overflowBodySize =
        static_cast<size_t>(
            std::numeric_limits<uint16_t>::max()
        )
        - sizeof(PacketHeader)
        + 1;

    const std::string body(overflowBodySize, 'A');

    RequireLengthError(
        [&]()
        {
            PacketParser::MakePacket(123, body);
        },
        "uint16 packet length overflow was accepted"
    );
}
}

int main()
{
    try
    {
        TestThreeHundredByteRoundTrip();
        TestEmptyFieldRoundTrip();
        TestTruncatedLengthHeader();
        TestDeclaredLengthOverflow();
        TestMakePacketRoundTrip();
        TestMaximumPacketSizeAccepted();
        TestPacketSizeOverflowRejected();
        TestUint16PacketLengthOverflowRejected();
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return 1;
    }

    std::cout << "[PASS] PacketParser length-prefix tests\n";
    return 0;
}