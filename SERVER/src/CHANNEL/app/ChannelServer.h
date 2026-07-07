#pragma once

#include "ChannelSession.h"
#include "PlayerManager.h"
#include "PlayerService.h"
#include "MapService.h"
#include "MapManager.h"
#include "MonsterManager.h"
#include "MySqlConnectionPool.h"
#include "RedisConnectionPool.h"
#include "RedisClient.h"
#include "common.h"

#include "StatService.h"
#include "ItemManager.h"
#include "SkillManager.h"
#include "ItemService.h"
#include "DropManager.h"

#include "ThreadPool.h"
#include "CommandReceiver.h"
#include "CombatService.h"
#include "TradeService.h"
#include "LevelManager.h"
#include "ChannelAuthResult.h"

#include <atomic>
#include <queue>
#include <mutex>
#include <cstdint>

class ChannelServer
{
public:
    ChannelServer(const int channelId, const int threadCount, const int maxUserCount);
    ~ChannelServer();

    bool Init(const int port, const RedisConfig& redisConfig);
    void Run();

    void OnReceive(int fd);
    void OnDisconnect(int fd);
    void BroadCast(); // 매개변수로 packet 받아야함
    void EnableWriteEvent(int fd);
    void PushAuthResult(ChannelAuthResult result);
    ChannelSession* FindValidSession(int fd, uint64_t sessionId, uint64_t generation);
    ChannelSession* BeginValidSessionTask(int fd, uint64_t sessionId, uint64_t generation);
    void EndSessionTask(ChannelSession* session);

public:
    PlayerManager* GetPlayerManager() { return &m_player_mamager; }
    MapService* GetMapService() {return &m_map_service;}
    PlayerService* GetPlayerService() {return &m_player_service;}
    StatService* GetStatService() {return &m_stat_service;}
    ItemService* GetItemService() {return &m_item_service;}
    CombatService* GetCombatService() {return &m_combat_service;}
    TradeService* GetTradeService() {return &m_trade_service;}
    ThreadPool* GetThreadPool() {return &m_pool;}
    ThreadPool* GetAuthThreadPool() { return &m_authPool; }
    std::mutex& GetAuthLoadMutex() { return m_authLoadMutex; }
    RedisConnectionPool* GetRedisConnectionPool() { return &m_redisPool; }
    void UpdateChannelState(const int interval, const int ttl);
    void UpdateChannelStateToRedis(const int ttl);
private:
    bool InitListenSocket(int port);
    bool InitEpoll();

    void GameLoop();
    void OnAccept();

    static int SetNonblocking(int fd);

    void DisableWriteEvent(int fd);
    void OnSend(int fd);
    void ProcessAuthResults();

private:
    int m_channel_id;
    int m_listen_fd;
    int m_epfd;
    bool m_running;

    std::vector<epoll_event> m_events;
    std::unordered_map<int,ChannelSession*> m_sessions;
    PlayerManager m_player_mamager;
    PlayerService m_player_service;
    MapManager m_map_manager;
    ItemManager* m_item_manager;
    MonsterManager* m_monster_manager;
    SkillManager* m_skill_manager;
    DropManager* m_drop_manager;
    
    
    MapService m_map_service;
    StatService m_stat_service;
    ItemService m_item_service;
    CombatService m_combat_service;

    ThreadPool m_pool;
    // ChannelAuth 전용 쓰레드
    ThreadPool m_authPool;
    RedisConnectionPool m_redisPool;
    CommandReceiver m_cmd_receiver;

    TradeService m_trade_service;

    LevelManager* m_level_manager;

    std::queue<ChannelAuthResult> m_authResults;
    std::atomic<uint64_t> m_nextSessionId{1};
    std::mutex m_authResultMutex;
    std::mutex m_authLoadMutex;
    std::mutex m_sessionMutex;
    std::atomic<unsigned int> m_current_user_count;
    unsigned int m_max_user_count;
};
