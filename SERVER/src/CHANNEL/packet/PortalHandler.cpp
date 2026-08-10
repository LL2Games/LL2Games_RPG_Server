#include "common.h"
#include "PortalHandler.h"
#include "ChannelSession.h"
#include "PacketParser.h"
#include "K_slog.h"
#include "utility.h"
#include "MapService.h"
#include "MapData.h"
#include "PortalPacketSender.h"
#include "MapInstance.h"



void PortalHandler::Execute(PacketContext * ctx)
{
    ChannelSession *session = nullptr;
    Player* player = nullptr;
    MapInstance* currentMap = nullptr;
    int rc = EXIT_SUCCESS;

    size_t offset = 0;
    std::string errMsg;
    std::string portalId;
    PortalMoveResult MoveResult{};

    if(ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }

    session = ctx->channel_session;
    if(session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    player = session->GetPlayer();

    if(player ==nullptr)
    {
        K_LOG_ERROR("player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }

    currentMap = player->GetCurrentMap();

    if(currentMap == nullptr)
    {
        K_LOG_ERROR("currentMap is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]currentMap is nullptr";
        goto err;
    }


    if (!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        portalId,
        errMsg))
    {
        K_LOG_ERROR( "ParseLengthPrefixedString failed\n");
        rc = EXIT_FAILURE;
        goto err;
    }

    MoveResult = ctx->map_service->MoveByPortal(player ,portalId);

    if(!MoveResult.success)
    {
        K_LOG_ERROR("MoveByPortal failed errorMSG [%s]\n", MoveResult.error.c_str());
        rc = EXIT_FAILURE;
        goto err;
    }
  
    
err:
    if (rc != EXIT_SUCCESS) {
        const std::string responseError = !MoveResult.error.empty()? MoveResult.error : errMsg;
        session->SendNok(PKT_PORTAL_ENTER, MoveResult.error);
    } else {
        MapInstance* destinationMap = player->GetCurrentMap();

        if (destinationMap == nullptr)
        {
            K_LOG_ERROR("Destination map is nullptr after portal movement. playerId[%d]", player->GetId());
            session->SendNok(PKT_PORTAL_ENTER, "Destination map is not available");
            return;
        }

        // 클라이언트가 목적지 씬으로 먼저 전환하도록 성공 응답을 전송한다.
        PortalPacketSender::SendMoveSuccess(session, MoveResult.destinationMapId,MoveResult.spawnPosition);

        // 씬 전환 응답 뒤 목적지 맵의 플레이어·몬스터 정보를 전송한다.
        destinationMap->SendEnterPackets(player);

        K_LOG_TRACE("Portal movement completed. playerId[%d] destinationMapId[%d]",player->GetId(),MoveResult.destinationMapId);
    }
}

