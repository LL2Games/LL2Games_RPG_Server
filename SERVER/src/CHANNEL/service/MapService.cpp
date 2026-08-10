#include "MapService.h"
#include "Player.h"
#include "MapInstance.h"
#include "MapManager.h"

MapService::MapService(PlayerManager& playermanager, MapManager& mapManager) : m_playerManager(playermanager), m_MapManger(mapManager)
{

}

int MapService::EnterMap(int playerId, int mapID)
{
    Player* player;
    MapInstance* map;
    player = m_playerManager.GetPlayer(playerId);
    if(player == nullptr) 
    {
        K_LOG_ERROR( "Player is nullptr");
        return EXIT_FAILURE;
    }
    // 맵 정보 로드 및 생성
    map = m_MapManger.GetOrCreate(mapID);

    if (map == nullptr)
        return EXIT_FAILURE;
    // 플레이어 현재 맵 설정
    player->SetCurrentMap(map);

    // 맵 Enter 로직 수행
    map->OnEnter(playerId, player);

    return EXIT_SUCCESS;
}

PortalMoveResult MapService::MoveByPortal(Player* player, const std::string& portalId)
{
    PortalMoveResult moveResult{};

    if(player == nullptr)
    {
        moveResult.error = "player is nullptr";
        return moveResult;
    }

    MapInstance* currentMap = player->GetCurrentMap();

    if(currentMap == nullptr)
    {
        moveResult.error = "current map is nullptr";
        return moveResult;
    }

    std::optional<PortalData> portal = currentMap->FindPortal(portalId);

    if(!portal.has_value())
    {
        moveResult.error = "portal not found";
        return moveResult;
    }

    const Vec2 playerPosition = player->GetPos();

    if (!portal->IsInInteractionRange(playerPosition))
    {
        const float differenceX = playerPosition.xPos - portal->position.xPos;
        const float differenceY = playerPosition.yPos - portal->position.yPos;
        const float distanceSquared = differenceX * differenceX + differenceY * differenceY;

        K_LOG_ERROR("Portal range check failed. playerId[%d] portalId[%s] playerPos[%.3f, %.3f] portalPos[%.3f, %.3f] range[%.3f] distanceSquared[%.3f]",
            player->GetId(),
            portalId.c_str(),
            playerPosition.xPos,
            playerPosition.yPos,
            portal->position.xPos,
            portal->position.yPos,
            portal->interactionRange,
            distanceSquared
        );

        moveResult.error = "portal is out of range";
        return moveResult;
    }   

    if(portal->destinationMapId <= 0)
    {
        moveResult.error = "invalid destination map";
        return moveResult;
    }

    MapInstance* destinationMap = m_MapManger.GetOrCreate(portal->destinationMapId);

    if(destinationMap == nullptr)
    {
        moveResult.error = "destination map load failed";
        return moveResult;
    }

    const int playerId = player->GetId();

    currentMap->OnLeave(playerId);

    player->SetMapId(portal->destinationMapId);

    player->SetPos(portal->spawnPosition);

    player->SetCurrentMap(destinationMap);

    destinationMap->OnEnter(playerId, player);


    moveResult.success = true;
    moveResult.destinationMapId = portal->destinationMapId;
    moveResult.spawnPosition = portal->spawnPosition;

    return moveResult;

}