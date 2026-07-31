#include "Packet.h"
#include "PacketParser.h"
#include <cstring>
#include <type_traits>
#include "K_slog.h"
#include "utility.h"
#include <limits>
#include <stdexcept>

std::optional<ParsedPacket> PacketParser::Parse(std::vector<char>& buf)
{
    ParsedPacket parsedPacket;
    if (buf.size() < sizeof(PacketHeader))
    {
        K_LOG_ERROR( "buf.size() < sizeof(PacketHeader)");
        K_LOG_ERROR( "sizeof(PacketHeader)[%d]", sizeof(PacketHeader));
        K_LOG_ERROR( "buf.size[%d]", buf.size());
        return std::nullopt;
    }

    PacketHeader *hdr = reinterpret_cast<PacketHeader *>(buf.data());
    uint16_t pktLen = hdr->length;

    if (buf.size() < pktLen)
    {
        K_LOG_ERROR( "buf.size small pktLen");
        return std::nullopt;
    }
    uint16_t type = hdr->type;

    const char *payload = reinterpret_cast<const char *>(buf.data() + sizeof(PacketHeader));
    int payloadLen = pktLen - sizeof(PacketHeader);
    
    parsedPacket.type = type;
    parsedPacket.payload = std::string(payload, payloadLen);

    buf.erase(buf.begin(), buf.begin() + pktLen);

    return parsedPacket;
}

ParseResult PacketParser::TryParse(std::vector<char> &buf)
{
        ParsedPacket parsedPacket;

    if (buf.size() < sizeof(PacketHeader))
    {
        return { ParseStatus::NeedMoreData, {} };
    }

    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(buf.data());
    uint16_t pktLen = hdr->length;

    if (pktLen < sizeof(PacketHeader))
    {
        return { ParseStatus::InvalidPacket, {} };
    }

    if (pktLen > BUFFER_SIZE)
    {
        return { ParseStatus::InvalidPacket, {} };
    }

    if (buf.size() < pktLen)
    {
        return { ParseStatus::NeedMoreData, {} };
    }

    uint16_t type = hdr->type;

    const char* payload = reinterpret_cast<const char*>(buf.data() + sizeof(PacketHeader));
    int payloadLen = pktLen - sizeof(PacketHeader);

    parsedPacket.type = type;
    parsedPacket.payload = std::string(payload, payloadLen);

    buf.erase(buf.begin(), buf.begin() + pktLen);

    return { ParseStatus::Complete, parsedPacket };
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
    uint16_t value_len = 0;
    std::memcpy(&value_len, payload + offset, sizeof(value_len));
    offset += sizeof(value_len);

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
        body.append(reinterpret_cast<const char*>(&dataLen), sizeof(dataLen));
        body.append(data);
    }
    return body;
}

std::string PacketParser::MakePacket(uint16_t type, const std::string &body)
{
   const size_t maximumPacketSize = std::min(
        static_cast<size_t>(14096),
        static_cast<size_t>(std::numeric_limits<uint16_t>::max())
    );

    if (maximumPacketSize < sizeof(PacketHeader) || body.size() > maximumPacketSize - sizeof(PacketHeader))
    {
        throw std::length_error("packet is too large");
    }

    const size_t packetLength = sizeof(PacketHeader) + body.size();

    PacketHeader hdr{};
    hdr.type = type;
    hdr.length = static_cast<uint16_t>(packetLength);

    std::string packet;
    packet.reserve(packetLength);
    packet.append(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    packet.append(body);

    return packet;
}

