#pragma once

#include "Task.h"
#include "Packet.h"
#include "ChannelPacketFactory.h"

#include <cstdint>
#include <string>

class ChannelServer;
class ChannelSession;

class PacketProcessTask : public Task
{
public:
    PacketProcessTask(ChannelServer* server, ChannelSession* session, int fd, uint64_t sessionId, uint64_t generation, uint16_t type, std::string payload);

    void Execute() override;

private:
    ChannelServer* m_server;
    ChannelSession* m_session;
    int m_fd;
    uint64_t m_sessionId;
    uint64_t m_generation;
    uint16_t m_type;
    std::string m_payload;
    ChannelPacketFactory m_factory;
};