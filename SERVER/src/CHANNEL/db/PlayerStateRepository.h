#pragma once

#include "MySqlConnectionPool.h"
#include "PlayerSaveData.h"

#include <string>

class PlayerStateRepository
{
public:
    PlayerStateRepository();
    bool Save(const PlayerSaveData& saveData,std::string& errMsg);

private:
    bool SaveCharacter(MYSQL* connection,const PlayerSaveData& saveData,std::string& errMsg);
    bool SaveCharacterStat(MYSQL* connection,const PlayerSaveData& saveData,std::string& errMsg);
    bool SaveInventory(MYSQL* connection,const PlayerSaveData& saveData,std::string& errMsg);
    bool SaveQuickSlots(MYSQL* connection,const PlayerSaveData& saveData,std::string& errMsg);

private:
    MySqlConnectionPool* m_mySql = nullptr;
};