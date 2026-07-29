#include "Packet.h"
#include "PacketParser.h"
#include "PacketFactory.h"
#include <cstring>
#include <limits>
#include <stdexcept>
#include "K_slog.h"

std::optional<ParsedPacket> PacketParser::Parse(std::vector<char>& buf)
{
    ParsedPacket parsedPacket;
    if(buf.size() < sizeof(PacketHeader))
    {
        K_LOG_ERROR( "buf.size() < sizeof(PacketHeader)");
        return std::nullopt;
    }

    PacketHeader *hdr = reinterpret_cast<PacketHeader *>(buf.data());
    uint16_t pktLen = hdr->length;  // 네트워크 바이트 순서 변환 추가

    if(buf.size() < pktLen)
    {
        K_LOG_ERROR( "buf.size small pktLen");
        return std::nullopt;
    }

    uint16_t type = hdr->type;

    const char *payload = reinterpret_cast<const char*>(buf.data() + sizeof(PacketHeader));
    int payloadLen = pktLen - sizeof(PacketHeader);

    parsedPacket.type = type;
    parsedPacket.payload = std::string(payload, payloadLen);
    buf.erase(buf.begin(), buf.begin() + pktLen);

    return parsedPacket;
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
    PacketHeader hdr;
    std::string packet;
    hdr.type = type;
    hdr.length = sizeof(PacketHeader) + body.size();
    packet.append((char *)&hdr, sizeof(hdr));
    packet.append(body);
    return packet;
}