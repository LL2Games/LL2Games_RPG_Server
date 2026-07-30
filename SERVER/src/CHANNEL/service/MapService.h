#pragma once
#include "common.h"
#include "PlayerManager.h"
#include "MapData.h"

class MapManager;
class MapService
{
public:
    MapService(PlayerManager& playerManager, MapManager& mapManager);
    ~MapService(){};

    // player가 포탈을 통해 map에 들어왔을 때 수행되는 함수
    int EnterMap(int playerId, int mapID);
    PortalMoveResult MoveByPortal(Player* player, const std::string& portalId);

private:
    PlayerManager& m_playerManager;
    MapManager& m_MapManger;
};