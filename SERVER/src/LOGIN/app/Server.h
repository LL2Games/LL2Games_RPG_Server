#pragma once
#include <vector>
#include <sys/select.h>
#include <atomic>
#include "Client.h"
#include "LoginPacketFactory.h"
#include "RedisConnectionPool.h"
#include "Packet.h"
#include <memory>
#include <openssl/ssl.h>
// Reactor

class Server
{
public:
    bool Init(int port, const RedisConfig& redisConfig, const char* certPath, const char* keyPath);
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

private:
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> m_tlsContext{nullptr, SSL_CTX_free};
};