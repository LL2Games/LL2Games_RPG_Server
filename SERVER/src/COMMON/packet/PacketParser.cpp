#include "Packet.h"
#include "PacketParser.h"
#include <cstring>
#include <type_traits>
#include "K_slog.h"
#include "utility.h"
#include <limits>
#include <stdexcept>
#include <arpa/inet.h>

ParseResult PacketParser::TryParse(std::vector<char> &buf)
{
    if (buf.size() < sizeof(PacketHeader))
    {
        return { ParseStatus::NeedMoreData, {} };
    }

    PacketHeader header{};

    std::memcpy(&header, buf.data(), sizeof(header));

    const uint16_t packetLength = ntohs(header.length);
    
    if (packetLength < sizeof(PacketHeader))
    {
        return { ParseStatus::InvalidPacket, {} };
    }

    if (packetLength > PacketLimits::kMaxPacketSize)
    {
        return { ParseStatus::InvalidPacket, {} };
    }

    if (buf.size() < packetLength)
    {
        return { ParseStatus::NeedMoreData, {} };
    }

    ParsedPacket parsedPacket;
    parsedPacket.type = ntohs(header.type);
   
    const char* payload = buf.data() + sizeof(PacketHeader);
    const std::size_t payloadLength = packetLength - sizeof(PacketHeader);

    parsedPacket.payload.assign(payload, payloadLength);

    buf.erase(buf.begin(), buf.begin() + packetLength);

    return { ParseStatus::Complete, std::move(parsedPacket)};
}

bool PacketParser::ParseLengthPrefixedString(
    const char *payload,
    const size_t payload_len,
    size_t &offset,
    std::string &outValue,
    std::string &errMsg)
{
    if (payload == nullptr)
    {
        errMsg = "payload empty";
        return false;
    }

    if (offset > payload_len || payload_len - offset < sizeof(uint16_t))
    {
        errMsg = "field length header overflow";
        return false;
    }

    // Length prefix: uint16_t (2 bytes)
    uint16_t networkValueLength = 0;
    std::memcpy(&networkValueLength, payload + offset, sizeof(networkValueLength));
    offset += sizeof(networkValueLength);

    const uint16_t value_len = ntohs(networkValueLength);

    if(payload_len - offset < value_len)
    {
        errMsg = "payload length overflow";
        return false;
    }

    // 4. extract value
    outValue.assign(payload + offset, value_len);
    offset += value_len;

    return true;
}

bool PacketParser::ParseNextIntField(const char* data, size_t payloadSize, size_t& offset, int& outValue, std::string& errMsg)
{
    std::string temp;

    if (!PacketParser::ParseLengthPrefixedString(
        data,
        payloadSize,
        offset,
        temp,
        errMsg))
    {
        return false;
    }

    if (!utility::StringToInt(temp, outValue))
    {
        errMsg = "StringToInt failed: " + temp;
        return false;
    }

    return true;
}

bool PacketParser::ParseNextFloatField(const char* data, size_t payloadSize, size_t& offset, float& outValue, std::string& errMsg)
{
    std::string temp;

    if (!PacketParser::ParseLengthPrefixedString(
        data,
        payloadSize,
        offset,
        temp,
        errMsg))
    {
        return false;
    }

    if (!utility::StringToFloat(temp, outValue))
    {
        errMsg = "StringToFloat failed: " + temp;
        return false;
    }

    return true;
}


std::string PacketParser::MakeBody(const std::vector<std::string>& datas)
{
     std::string body;
    for (const auto& data : datas)
    {
        if(data.size() > std::numeric_limits<uint16_t>::max())
        {
            throw std::length_error("packet field is too larger");
        }
        uint16_t dataLen = static_cast<uint16_t>(data.size());
        const uint16_t networkDataLength = htons(dataLen);
        body.append(reinterpret_cast<const char*>(&networkDataLength), sizeof(networkDataLength));
        body.append(data);
    }
    return body;
}

std::string PacketParser::MakePacket(uint16_t type, const std::string &body)
{

    // 미리 정해놓은 최대 패킷 사이즈를 가져온다
    constexpr std::size_t maxPacketSize = PacketLimits::kMaxPacketSize;

    // 최대 패킷 크기가 패킷 헤더보다 작은 설정을 컴파일 단계에서 방지한다.
    static_assert(maxPacketSize >= sizeof(PacketHeader), "Maximum  packet size is smaller than packet header");
    
    const std::size_t maxBodySize = maxPacketSize - sizeof(PacketHeader);

    if(body.size() > maxBodySize)
    {
        throw std::length_error("packet is too large");
    }

    const std::size_t packetLength = sizeof(PacketHeader) + body.size();
   
    PacketHeader hdr{};
    hdr.type = htons(type);
    hdr.length =  htons(static_cast<uint16_t>(packetLength));

    std::string packet;
    packet.reserve(packetLength);
    packet.append(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    packet.append(body);

    return packet;
}

