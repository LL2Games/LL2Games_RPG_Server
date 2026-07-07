#pragma once
#include "common.h"
#include "CommonEnum.h"
#include <unordered_map>
#include <memory>
#include "Player.h"
#include "MySqlConnectionPool.h"
#include <mutex>
class PlayerManager
{   
public:
    PlayerManager();
    ~PlayerManager();

    bool Init();
    bool AddPlayer(std::unique_ptr<Player> player);

    bool MovePlayer(int playerID, Vec2 pos, float speed);
    bool RemovePlayer(int playerId);
public:
    Player* GetPlayer(int playerId);
    Player* GetPlayer(const std::string& playerName);


private:
    std::unordered_map<int, std::unique_ptr<Player>> m_players;
    MySqlConnectionPool* m_mySql;
    std::mutex m_PlayerMutex;

};
