#include "common.h"
#include "MapHandler.h"
#include "ChannelSession.h"
#include "PacketParser.h"
#include "K_slog.h"
#include "utility.h"
#include "Player.h"
#include "MapInstance.h"

#include <sstream>
#include <stdexcept>


void MapHandler::Execute(PacketContext * ctx)
{
   if (ctx == nullptr)
    {
        K_LOG_ERROR("MapHandler: context is nullptr");
        return;
    }

    ChannelSession* session = ctx->channel_session;

    if (session == nullptr)
    {
        K_LOG_ERROR("MapHandler: session is nullptr");
        return;
    }

    Player* player = session->GetPlayer();

    if (player == nullptr)
    {
        session->SendNok(PKT_ENTER_MAP,"Authenticated player is not available");
        return;
    }

    if (ctx->map_service == nullptr)
    {
        session->SendNok(PKT_ENTER_MAP,"Map service is not available");
        return;
    }

    // 클라이언트가 보낸 ID가 아니라 인증된 세션의 플레이어 정보를 사용한다.
    const int playerId = player->GetId();
    const int mapId = player->GetMapId();

    if (mapId <= 0)
    {
        session->SendNok(PKT_ENTER_MAP,"Saved map ID is invalid");
        return;
    }

    if (ctx->map_service->EnterMap(playerId,mapId) != EXIT_SUCCESS)
    {
        session->SendNok(PKT_ENTER_MAP,"EnterMap failed");
        return;
    }

   MapInstance* map = player->GetCurrentMap();

    if (map == nullptr)
    {
        session->SendNok(PKT_ENTER_MAP, "Current map is not available after entering map");
        return;
    }

    const Vec2 position = player->GetPos();

    // 클라이언트가 먼저 맵 전환을 처리하도록 성공 응답을 먼저 전송한다.
    session->SendOk(PKT_ENTER_MAP,
        {   std::to_string(mapId),
            std::to_string(position.xPos),
            std::to_string(position.yPos)
        }
    );

    // 맵 전환 응답 뒤 기존 플레이어와 몬스터 정보를 전송한다.
    map->SendEnterPackets(player);

    K_LOG_TRACE(
        "Player entered saved map. playerId[%d] mapId[%d]",
        playerId,
        mapId
    );
}

