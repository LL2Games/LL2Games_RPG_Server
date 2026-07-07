#pragma once
#include <vector>
#include <sys/select.h>
#include "Client.h"
#include "LoginPacketFactory.h"
#include "RedisConnectionPool.h"

// Reactor

class Server
{
public:
    bool Init(int port, const RedisConfig& redisConfig);
    void Run();

private:
    int m_listen_fd;
    std::vector<Client*> m_clients;
    LoginPacketFactory m_factory;

    void AcceptNewClient();
    void ProcessClient(Client* cli);

    RedisConnectionPool m_redisPool;
};