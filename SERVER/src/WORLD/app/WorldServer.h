#pragma once
#include <string>
#include <map>
#include <atomic>
#include "MySqlConnectionPool.h"
#include "RedisClient.h"
#include "WorldSession.h"
#include "ChannelManager.h"
#include "CharacterService.h"
#include "WorldPacketFactory.h"
#include "RedisConnectionPool.h"

class WorldServer
{
public:
    WorldServer();
    ~WorldServer();
    int Init(const std::string& configPath);
    int Init(const int port, const RedisConfig& redisConfig);
    int Run();
    int OnAccept();
    int OnReceive(int fd);
    int OnDisconnect(int fd);
    int HandleSelectCharacter(int fd, const std::string& charId);
    int HandleChannelHeartBeat(const std::string& pkt);

    void RequestStop() noexcept;
    void ShutdownGracefully() noexcept;
public:
    RedisConnectionPool* GetRedisConnectionPool() { return &m_redisPool; }

private:
    void BroadcastServerShutdown();
    void DisconnectAllClients() noexcept;

private:
    int m_listen_fd = -1;
    std::map<int, WorldSession*> m_sessions;
    ChannelManager  m_channel_manager;
    CharacterService m_char_service;
    WorldPacketFactory m_factory;
    RedisConnectionPool m_redisPool;

    std::atomic_bool m_running{false};
};