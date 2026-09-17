#pragma once
#include <vector>
#include <sys/select.h>
#include <atomic>
#include "Client.h"
#include "ChatPacketFactory.h"
#include "CommandDispatcher.h"
#include "RedisConnectionPool.h"

class Server {
public:
    Server();
    bool Init(const int port, const RedisConfig& redisConfig);
    void Run();

    void RequestStop() noexcept;
    void ShutdownGracefully();
private:

    void AcceptNewClient();
    void ProcessClient(Client* cli);
    void DisconnectClient(Client* client);
    void DisconnectAllClients() noexcept;
    void BroadCast(const std::string& nick, const std::string& msg, const int exceptFd = -1);

private:
    int m_listenFd = -1;
    std::atomic_bool m_running{false};
    std::vector<Client *> m_clients;
    ChatPacketFactory m_factory;
    CommandDispatcher m_dispatcher;

    
    RedisConnectionPool m_redisPool;
};