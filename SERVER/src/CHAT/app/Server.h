#pragma once
#include <vector>
#include <sys/select.h>
#include "Client.h"
#include "ChatPacketFactory.h"
#include "CommandDispatcher.h"
#include "RedisConnectionPool.h"

class Server {
public:
    Server();
    bool Init(const int port, const RedisConfig& redisConfig);
    void Run();
    
private:
    int m_listenFd;
    std::vector<Client *> m_clients;
    ChatPacketFactory m_factory;
    CommandDispatcher m_dispatcher;

    void AcceptNewClient();
    void ProcessClient(Client* cli);
    void BroadCast(const std::string& nick, const std::string& msg, const int exceptFd = -1);
    RedisConnectionPool m_redisPool;
};