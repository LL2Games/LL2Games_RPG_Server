#include "PlayerHandler.h"
#include "common.h"
#include "PacketParser.h"
#include "ChannelSession.h"
#include "PlayerManager.h"
#include "MapInstance.h"
#include "utility.h"


void PlayerHandler::MovePacket(PacketContext * ctx)
{
    ChannelSession *session = nullptr;
    Player* player = nullptr;
    MapInstance* map = nullptr;

    size_t offset = 0;
    int rc = EXIT_SUCCESS;
    float speed = 0;

    std::string errMsg;
    Vec2 PlayerPos = {0,0};
    int dir = 0;
     
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
    if(player == nullptr) 
    {
        K_LOG_ERROR( "Player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]Player is nullptr";
        goto err;
    }

    map = player->GetCurrentMap();
     if(map == nullptr) 
    {
        K_LOG_ERROR( "current_map is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]current_map is nullptr";
        goto err;
    }

    // 받은 정보에서 Xpos 추출
    if(!PacketParser::ParseNextFloatField(ctx->payload, ctx->payload_len, offset, PlayerPos.xPos, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextFloatField fail");
        goto err;
    }

     // 받은 정보에서 Ypos 추출
    if(!PacketParser::ParseNextFloatField(ctx->payload, ctx->payload_len, offset, PlayerPos.yPos, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextFloatField fail");
        goto err;
    }

    // 받은 정보에서 speed 추출
    if(!PacketParser::ParseNextFloatField(ctx->payload, ctx->payload_len, offset, speed, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextFloatField fail");
        goto err;
    }

    // 받은 정보에서 Dir 추출
    if(!PacketParser::ParseNextIntField(ctx->payload, ctx->payload_len, offset, dir, errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseNextIntField fail");
        goto err;
    }
    

    // 플레이어가 속한 맵의 모든 플레이어한테 변경 정보 전달
    map->HandleMove(player, PlayerPos, speed, dir);
   
err:
    if (rc != EXIT_SUCCESS) {
        session->SendNok(PKT_PLAYER_MOVE, errMsg);
    } else {
        session->SendOk(PKT_PLAYER_MOVE);
    }
}