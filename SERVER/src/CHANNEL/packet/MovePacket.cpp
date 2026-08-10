#include "PlayerHandler.h"
#include "common.h"
#include "PacketParser.h"
#include "ChannelSession.h"
#include "PlayerManager.h"
#include "MapInstance.h"
#include "utility.h"


void PlayerHandler::MovePacket(PacketContext* ctx)
{
    ChannelSession* session = nullptr;
    Player* player = nullptr;
    MapInstance* map = nullptr;

    size_t offset = 0;
    int rc = EXIT_SUCCESS;

    float receivedSpeed = 0.0f;
    float serverSpeed = 0.0f;

    std::string errMsg;
    Vec2 playerPos = {0, 0};
    int dir = 0;

    if (ctx == nullptr)
    {
        K_LOG_ERROR("ctx is nullptr");
        rc = EXIT_FAILURE;
        errMsg ="[" + std::to_string(rc) +"]ctx is nullptr";
        goto err;
    }

    session = ctx->channel_session;

    if (session == nullptr)
    {
        K_LOG_ERROR("session is nullptr");
        rc = EXIT_FAILURE;
        errMsg ="[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    player = session->GetPlayer();

    if (player == nullptr)
    {
        K_LOG_ERROR("Player is nullptr");
        rc = EXIT_FAILURE;
        errMsg ="[" + std::to_string(rc) + "]Player is nullptr";
        goto err;
    }

    map = player->GetCurrentMap();

    if (map == nullptr)
    {
        K_LOG_ERROR("current_map is nullptr");
        rc = EXIT_FAILURE;
        errMsg ="[" + std::to_string(rc) + "]current_map is nullptr";
        goto err;
    }

    // X 좌표
    if (!PacketParser::ParseNextFloatField(ctx->payload,ctx->payload_len, offset, playerPos.xPos,errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR("ParseNextFloatField X fail");

        goto err;
    }

    // Y 좌표
    if (!PacketParser::ParseNextFloatField(ctx->payload, ctx->payload_len, offset, playerPos.yPos, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR("ParseNextFloatField Y fail");

        goto err;
    }

    // 기존 패킷 규격 유지를 위해 파싱하지만 신뢰하지 않는다.
    if (!PacketParser::ParseNextFloatField(ctx->payload, ctx->payload_len, offset, receivedSpeed, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR("ParseNextFloatField speed fail");

        goto err;
    }

    // 방향
    if (!PacketParser::ParseNextIntField(ctx->payload, ctx->payload_len, offset, dir, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR("ParseNextIntField dir fail");
        goto err;
    }

    // 서버 기준으로 이동 가능 여부를 검증한다.
    if (!player->TryApplyMove(playerPos,errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR("Movement rejected. playerId[%d] reason[%s]", player->GetId(),errMsg.c_str());
        goto err;
    }

    // 클라이언트가 보낸 속도 대신 서버 속도를 사용한다.
    serverSpeed = player->GetMoveSpeed();

    // 검증된 위치만 맵의 다른 플레이어에게 전달한다.
    map->HandleMove(player, playerPos, serverSpeed,dir);

err:
   if (rc != EXIT_SUCCESS)
    {
        if (session == nullptr)
        {
            return;
        }

        if (player != nullptr)
        {
            const Vec2 serverPosition = player->GetPos();

            session->SendNok(PKT_PLAYER_MOVE,errMsg,{std::to_string(serverPosition.xPos),std::to_string(serverPosition.yPos)});
        }
        else
        {
            session->SendNok(PKT_PLAYER_MOVE, errMsg);
        }

        return;
    }

    session->SendOk(PKT_PLAYER_MOVE);
}