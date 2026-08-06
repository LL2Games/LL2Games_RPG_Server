#pragma once

#include "Task.h"

#include <cstdint>

class ChannelServer;

class PlayerSaveTask : public Task
{
public:
    PlayerSaveTask(ChannelServer* server,int fd,std::uint64_t sessionId,std::uint64_t generation);
    void Execute() override;

private:
    ChannelServer* m_server = nullptr;
    int m_fd = -1;
    std::uint64_t m_sessionId = 0;
    std::uint64_t m_generation = 0;
};