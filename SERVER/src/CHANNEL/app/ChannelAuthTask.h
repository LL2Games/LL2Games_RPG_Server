#pragma once

#include "Task.h"
#include <string>
#include <cstdint>

class ChannelServer;

class ChannelAuthTask : public Task
{
public:
    ChannelAuthTask(ChannelServer* server, int fd, uint64_t sessionId, uint64_t generation, std::string payload);

    void Execute() override;

private:
    ChannelServer* m_server;
    int m_fd;
    uint64_t m_sessionId;
    uint64_t m_generation;
    std::string m_payload;
};