#include "PlayerManager.h"

PlayerManager::PlayerManager()
{
    m_mySql = MySqlConnectionPool::GetInstance();
}

PlayerManager::~PlayerManager()
{
}


bool PlayerManager::AddPlayer(std::unique_ptr<Player> player)
 {
     if (player == nullptr)
    {
        return false;
    }

    int playerId = player->GetId();

    std::lock_guard<std::mutex> lock(m_PlayerMutex);
    if(m_players.find(playerId) != m_players.end())
    {
        return false; 
    }
    //K_LOG_TRACE( "Player character_id [%d]", player->GetId());
    //K_LOG_TRACE( "Player character_id [%s]", player->GetName().c_str());

    m_players[playerId] = move(player);
    return true;
}




Player* PlayerManager::GetPlayer(int playerId)
{
    std::lock_guard<std::mutex> lock(m_PlayerMutex);
    auto it = m_players.find(playerId);

    if(it == m_players.end()) return nullptr;

    return it->second.get();
}

//추후에는 Redis먼저 조회 하고, Redis의 특정 주기에 따라서 DB조회를 해야함
//바로 DB조회 하는 방식은 리소스 손해를 많이본다.
Player* PlayerManager::GetPlayer(const std::string& playerName)
{
   std::lock_guard<std::mutex> lock(m_PlayerMutex);

    for (auto& [playerId, player] : m_players)
    {
        if (player != nullptr &&
            player->GetName() == playerName)
        {
            return player.get();
        }
    }

    return nullptr;
}

bool PlayerManager::RemovePlayer(int playerId)
{
    std::lock_guard<std::mutex> lock(m_PlayerMutex);
    auto it = m_players.find(playerId);

    if(it == m_players.end()) return false;

    m_players.erase(it);
    return true;
}