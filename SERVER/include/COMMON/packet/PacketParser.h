#pragma once
#include "Packet.h"
#include <functional>
#include <cstdint>
#include <vector>

class PacketParser {
public:
    static ParseResult TryParse(std::vector<char>& buf);
    static bool ParseLengthPrefixedString( const char *payload, const size_t payload_len, size_t &offset, std::string &outValue, std::string &errMsg);
    static bool ParseNextIntField(const char* data, size_t payloadSize, size_t& offset, int& outValue, std::string& errMsg);
    static bool ParseNextFloatField(const char* data, size_t payloadSize, size_t& offset, float& outValue, std::string& errMsg);


    static std::string MakeBody(const std::vector<std::string>& datas);
    static std::string MakePacket(uint16_t type, const std::string& body);

};