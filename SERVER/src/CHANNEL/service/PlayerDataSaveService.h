#pragma once

#include "Player.h"
#include "PlayerStateRepository.h"
#include "RedisConnectionPool.h"

#include <string>

class PlayerDataSaveService
{
public:
    // 변경된 플레이어만 주기적으로 저장
    bool SaveIfNeeded(Player& player,std::string& errMsg);
    // 접속 종료 시 최종 저장
    bool SaveNow(Player& player,std::string& errMsg);
    void SetRedisPool(RedisConnectionPool* redisPool);
private:
    bool Save(Player& player,bool forceSave,std::string& errMsg);
    bool InvalidatePlayerCache(int characterId,std::string& errMsg);
private:
    PlayerStateRepository m_repository;
    RedisConnectionPool* m_redisPool = nullptr;
};