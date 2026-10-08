#pragma once

#include "common.h"

#include "ChannelSession.h"
#include "CommandReceiver.h"
#include "ChannelAuthResult.h"
#include "PlayerSaveTask.h"
#include "MySqlConnectionPool.h"
#include "RedisConnectionPool.h"
#include "ThreadPool.h"
#include "RedisClient.h"

#include "MonsterManager.h"
#include "MapManager.h"
#include "PlayerManager.h"
#include "ItemManager.h"
#include "SkillManager.h"
#include "DropManager.h"
#include "LevelManager.h"
#include "NPCDataManager.h"
#include "ShopManager.h"

#include "MapService.h"
#include "PlayerService.h"
#include "StatService.h"
#include "CombatService.h"
#include "TradeService.h"
#include "ItemService.h"
#include "PlayerDataSaveService.h"
#include "NPCInteractionService.h"
#include "ShopService.h"

#include <atomic>
#include <queue>
#include <mutex>
#include <cstdint>
#include <chrono>
#include <unordered_set>
#include <thread>
#include <condition_variable>

class ChannelServerTestAccess;

class ChannelServer
{
    friend class ChannelServerTestAccess;
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

    bool SubmitFinalPlayerDataSave(PlayerSaveData saveData);
    bool IsFinalPlayerDataSavePending(int characterId) const;
    void CompleteFinalPlayerDataSave(int characterId,bool saveSucceeded,const std::string& errMsg);

    void RequestStop() noexcept;
public:
    PlayerManager* GetPlayerManager() { return &m_player_mamager; }
    NPCDataManager* GetNPCDataManager(){return &m_npcDataManager;}
    ShopManager* GetShopManager(){return m_shopManager;}
    
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
    PlayerDataSaveService* GetPlayerDataSaveService(){return &m_playerDataSaveService;}
    NPCInteractionService* GetNPCInteractionService(){return &m_npcInteractionService;}
    ShopService* GetShopService(){return &m_shopService;}
    int GetChannelId() const {return m_channel_id;}
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

    void SchedulePlayerSaves();

    void StartWorkers();
    void StopWorkers() noexcept;
    bool TryBeginRun() noexcept;

    void DisconnectAllSessions() noexcept;
    void BroadcastServerShutdown();
    void DrainSessionSendQueues();
private:
    int m_channel_id;
    int m_listen_fd;
    int m_epfd;

    std::vector<epoll_event> m_events;
    std::unordered_map<int,ChannelSession*> m_sessions;
    PlayerManager m_player_mamager;
    PlayerService m_player_service;
    MapManager m_map_manager;
    ItemManager* m_item_manager;
    MonsterManager* m_monster_manager;
    SkillManager* m_skill_manager;
    DropManager* m_drop_manager;
    NPCDataManager m_npcDataManager;
    ShopManager* m_shopManager = ShopManager::GetInstance();
    
    MapService m_map_service;
    StatService m_stat_service;
    ItemService m_item_service;
    CombatService m_combat_service;
    TradeService m_trade_service;
    LevelManager* m_level_manager;
    PlayerDataSaveService m_playerDataSaveService;
    NPCInteractionService m_npcInteractionService;
    ShopService m_shopService;

    ThreadPool m_pool;
    // ChannelAuth 전용 쓰레드
    ThreadPool m_authPool;
    // 최종 DB 저장 전용 스레드 풀
    // 동시 DB 저장 수를 제한하여 인증과 게임 작업을 보호한다.
    ThreadPool m_savePool;
    std::thread m_stateUpdateThread;
    RedisConnectionPool m_redisPool;
    CommandReceiver m_cmd_receiver;
    std::condition_variable m_stateUpdateCv;

   

    std::queue<ChannelAuthResult> m_authResults;
    std::atomic<uint64_t> m_nextSessionId{1};
    std::mutex m_authResultMutex;
    std::mutex m_authLoadMutex;
    std::mutex m_sessionMutex;
    std::mutex m_stateUpdateWaitMutex;
    mutable std::mutex m_finalPlayerDataSaveMutex;

    // 백그라운드 스레드가 시작됐는지
    std::atomic<bool> m_workersStarted{false};
    // 종료 요청이 들어왔는지
    std::atomic<bool> m_stopRequested{false};
    // 게임 루프 실행 여부
    std::atomic<bool> m_running{false};
    std::atomic<unsigned int> m_current_user_count;
    
    unsigned int m_max_user_count;
    std::unordered_set<int> m_finalPlayerDataSavePending;
};
