#pragma once

#include "Task.h"
#include <string>


class ChannelServer;

class ChannelAuthTask : public Task
{
public:
    ChannelAuthTask(ChannelServer* server, int fd, std::string payload);

    void Execute() override;

private:
    ChannelServer* m_server;
    int m_fd;
    std::string m_payload;
};