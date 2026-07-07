#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PacketTask {
    int fd = -1;
    uint64_t session_id = 0;
    uint64_t generation = 0;
    uint16_t packet_type = 0;
    std::string payload;
};

struct PacketResult {
    int fd = -1;
    uint64_t session_id = 0;
    uint64_t generation = 0;
    std::vector<std::string> send_packets;
    bool disconnect = false;
};