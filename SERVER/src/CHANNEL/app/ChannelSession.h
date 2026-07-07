#pragma once
#include "common.h"
#include "Player.h"
#include "PlayerManager.h"
#include "ChannelPacketFactory.h"
#include <deque>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <condition_variable>

class ChannelServer;

class ChannelSession
{
public:
    ChannelSession(int fd, ChannelServer* server, uint64_t sessionId, uint64_t generation);
    ~ChannelSession();


    int OnPacket(Packet packet);
    bool OnBytes(const uint8_t* data, size_t len);
   

    int Send(int type, const std::vector<std::string>& payload);
    //int Send(int type, std::vector<std::string> payload={});
    int SendOk(int type, std::vector<std::string> payload={});
    int SendNok(int type, const std::string &errMsg);
    int SendPacket(const std::string& packet);

    int EnqueueSend(std::string packet);
    bool FlushSend();
    bool HasPendingSend() const;
public: 
    void SetPlayer(Player* player) {m_player = player;}
    Player* GetPlayer() const {return m_player;}

    void SetPlayerManager(PlayerManager* playerManager) {m_playerManager = playerManager;}
    int GetFd() const {return m_fd;}
    void Dispatch(const ParsedPacket& pkt);

    uint64_t GetSessionId() const { return m_sessionId; }
    uint64_t GetGeneration() const { return m_generation; }
    bool TryBeginTask();
    void EndTask();
    void MarkClosing();
    void WaitForNoTasks();
    bool IsClosing() const { return m_closing.load(); }
private:
    int m_fd = -1;
    ChannelServer* m_server = nullptr;

    std::vector<char> m_recvBuf;

    Player* m_player;
    PlayerManager* m_playerManager;
    ChannelPacketFactory m_factory;

    std::deque<std::string> m_sendQueue;
    size_t m_sendOffset = 0;
    mutable std::mutex m_sendMutex;

    uint64_t m_sessionId = 0;
    uint64_t m_generation = 0;
    std::atomic<bool> m_closing{false};
    std::mutex m_taskMutex;
    std::condition_variable m_taskCv;
    int m_inFlightTasks = 0;
};
