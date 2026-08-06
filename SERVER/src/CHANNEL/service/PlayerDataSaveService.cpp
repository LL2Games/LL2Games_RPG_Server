#include "PlayerDataSaveService.h"


bool PlayerDataSaveService::SavePlayerData(const PlayerSaveData& saveData, std::string& errMsg)
{
    errMsg.clear();

    if (saveData.characterId <= 0)
    {
        errMsg = "Invalid character ID";
        return false;
    }

    if (!m_repository.Save(saveData, errMsg))
    {
        return false;
    }

    if (!InvalidatePlayerCache(saveData.characterId,errMsg))
    {
        // DB 저장은 성공했지만 Redis 캐시가 남아 있으므로
        // 호출자가 저장 성공으로 처리하지 않는다.
        return false;
    }

    return true;
}

bool PlayerDataSaveService::Save(Player& player, const bool forceSave, std::string& errMsg)
{
    errMsg.clear();

    if (!forceSave && !player.IsSaveNeeded())
    {
        return true;
    }

    const PlayerSaveData saveData = player.MakeSaveData();

    if (!SavePlayerData(saveData, errMsg))
    {
        // 실패하면 저장 버전을 유지하여 재시도할 수 있게 한다.
        return false;
    }

    // 저장하는 동안 상태가 다시 변경되지 않은 경우에만
    // 저장 대기 상태를 해제한다.
    player.TryMarkSaved(saveData.saveVersion);

    return true;
}

 // 변경된 플레이어만 주기적으로 저장
bool PlayerDataSaveService::SaveIfNeeded(Player& player,std::string& errMsg)
{
    return Save(player, false, errMsg);
}
// 접속 종료 시 최종 저장
bool PlayerDataSaveService::SaveNow(Player& player,std::string& errMsg)
{
    return Save(player, true, errMsg);
}

void PlayerDataSaveService::SetRedisPool(RedisConnectionPool* redisPool)
{
    m_redisPool = redisPool;
}

bool PlayerDataSaveService::InvalidatePlayerCache(const int characterId,std::string& errMsg)
{
    if (m_redisPool == nullptr)
    {
        errMsg = "Redis connection pool is not initialized";
        return false;
    }

    RedisConnectionGuard redisGuard(m_redisPool);

    if (!redisGuard)
    {
        errMsg = "Failed to acquire Redis connection";
        return false;
    }

    const std::string redisKey ="characterID:" + std::to_string(characterId);

    if (!redisGuard->Delete(redisKey))
    {
        errMsg ="Failed to invalidate player Redis cache";

        return false;
    }

    return true;
}
