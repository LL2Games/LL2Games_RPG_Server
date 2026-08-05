#include "PlayerDataSaveService.h"


bool PlayerDataSaveService::Save(Player& player,bool forceSave,std::string& errMsg)
{
    errMsg.clear();

    if (!forceSave && !player.IsSaveNeeded())
    {
        return true;
    }

    PlayerSaveData saveData = player.MakeSaveData();

    if (saveData.characterId <= 0)
    {
        errMsg = "Invalid character ID";
        return false;
    }

    if (!m_repository.Save(saveData, errMsg))
    {
        // 실패 시 저장 버전을 지우지 않는다.
        return false;
    }

    if (!InvalidatePlayerCache(saveData.characterId,errMsg))
    {
        // DB 저장은 성공했지만 Redis 캐시가 남아 있으므로
        // 저장 완료 상태로 바꾸지 않고 다음 주기에 재시도한다.
        return false;
    }
    // 저장 도중 상태가 바뀌지 않았을 때만 저장 완료 상태로 변경된다.
    // 상태가 변경됐다면 CAS가 실패하고 다음 주기에 다시 저장된다.
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
