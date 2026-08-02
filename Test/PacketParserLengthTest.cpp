#include "PacketParser.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <limits>
#include <vector>

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
      constexpr std::size_t maximumPacketSize =
        PacketLimits::kMaxPacketSize;

    static_assert(
        maximumPacketSize >= sizeof(PacketHeader),
        "Maximum packet size is smaller than packet header"
    );

    const std::size_t maximumBodySize =
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
    constexpr std::size_t maximumPacketSize =
        PacketLimits::kMaxPacketSize;

    const std::size_t overflowBodySize =
        maximumPacketSize - sizeof(PacketHeader) + 1;

    const std::string body(overflowBodySize, 'A');

    RequireLengthError(
        [&]()
        {
            PacketParser::MakePacket(123, body);
        },
        "packet larger than maximum size was accepted"
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

void TestTryParseMaximumPacketSizeAccepted()
{
    constexpr std::size_t maximumPacketSize =
        PacketLimits::kMaxPacketSize;

    const std::size_t maximumBodySize =
        maximumPacketSize - sizeof(PacketHeader);

    const std::string expectedBody(
        maximumBodySize,
        'A'
    );

    const std::string packet =
        PacketParser::MakePacket(123, expectedBody);

    std::vector<char> buffer(
        packet.begin(),
        packet.end()
    );

    ParseResult result =
        PacketParser::TryParse(buffer);

    Require(
        result.status == ParseStatus::Complete,
        "maximum-size packet was not parsed"
    );

    Require(
        result.packet.type == 123,
        "maximum-size packet type is incorrect"
    );

    Require(
        result.packet.payload == expectedBody,
        "maximum-size packet payload is incorrect"
    );

    Require(
        buffer.empty(),
        "maximum-size packet was not fully consumed"
    );
}

void TestTryParseOversizedPacketRejected()
{
    constexpr std::size_t oversizedPacketSize =
        PacketLimits::kMaxPacketSize + 1;

    static_assert(
        oversizedPacketSize <=
            std::numeric_limits<uint16_t>::max(),
        "test packet length does not fit in uint16_t"
    );

    PacketHeader header{};
    header.type = 123;
    header.length =
        static_cast<uint16_t>(oversizedPacketSize);

    std::vector<char> buffer(sizeof(PacketHeader));

    std::memcpy(
        buffer.data(),
        &header,
        sizeof(header)
    );

    const std::size_t originalBufferSize =
        buffer.size();

    ParseResult result =
        PacketParser::TryParse(buffer);

    Require(
        result.status == ParseStatus::InvalidPacket,
        "oversized receive packet was accepted"
    );

    Require(
        buffer.size() == originalBufferSize,
        "invalid packet unexpectedly modified the receive buffer"
    );
}

void TestTryParseCoalescedPackets()
{
    const std::string firstPacket =
        PacketParser::MakePacket(101, "first");

    const std::string secondPacket =
        PacketParser::MakePacket(102, "second");

    std::vector<char> buffer(
        firstPacket.begin(),
        firstPacket.end()
    );

    buffer.insert(
        buffer.end(),
        secondPacket.begin(),
        secondPacket.end()
    );

    ParseResult firstResult =
        PacketParser::TryParse(buffer);

    Require(
        firstResult.status == ParseStatus::Complete,
        "first coalesced packet was not parsed"
    );

    Require(
        firstResult.packet.type == 101,
        "first coalesced packet type is incorrect"
    );

    Require(
        firstResult.packet.payload == "first",
        "first coalesced packet payload is incorrect"
    );

    Require(
        !buffer.empty(),
        "second coalesced packet was consumed too early"
    );

    ParseResult secondResult =
        PacketParser::TryParse(buffer);

    Require(
        secondResult.status == ParseStatus::Complete,
        "second coalesced packet was not parsed"
    );

    Require(
        secondResult.packet.type == 102,
        "second coalesced packet type is incorrect"
    );

    Require(
        secondResult.packet.payload == "second",
        "second coalesced packet payload is incorrect"
    );

    Require(
        buffer.empty(),
        "coalesced packet buffer was not fully consumed"
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

        TestTryParseMaximumPacketSizeAccepted();
        TestTryParseOversizedPacketRejected();
        TestTryParseCoalescedPackets();
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return 1;
    }

    std::cout << "[PASS] PacketParser length-prefix tests\n";
    return 0;
}