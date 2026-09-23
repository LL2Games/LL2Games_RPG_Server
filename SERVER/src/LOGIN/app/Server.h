#pragma once
#include <vector>
#include <sys/select.h>
#include <atomic>
#include "Client.h"
#include "LoginPacketFactory.h"
#include "RedisConnectionPool.h"
#include "Packet.h"
// Reactor

class Server
{
public:
    bool Init(int port, const RedisConfig& redisConfig);
    void Run();

    void RequestStop() noexcept;
    void ShutdownGracefully();

private:
    void AcceptNewClient();
    void ProcessClient(Client* cli);
    void DisconnectClient(Client* client);

    void BroadcastServerShutdown() noexcept;
    void DisconnectAllClients() noexcept;

private:
    int m_listen_fd = -1;
    std::atomic_bool m_running{false};
    std::vector<Client*> m_clients;
    LoginPacketFactory m_factory;



    RedisConnectionPool m_redisPool;
};