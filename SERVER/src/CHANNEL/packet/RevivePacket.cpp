#include "PlayerHandler.h"
#include "common.h"
#include "PacketParser.h"
#include "Player.h"
#include "ChannelSession.h"
#include "MapInstance.h"

void PlayerHandler::HandleRevivePacket(PacketContext * ctx)
{
    ChannelSession *session = nullptr;
    Player *player = nullptr;
    MapInstance* map = nullptr;

    // size_t offset = 0;
    // int result = 0;
    int rc = EXIT_SUCCESS;
    std::string errMsg;
   
     K_LOG_DEBUG( "HandleRevivePacket Start\n");

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
     K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }

    map = player->GetCurrentMap();
    if (map == nullptr)
    {
     K_LOG_ERROR( "map is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]map is nullptr";
        goto err;
    }

    map->RevivePlayer(player);

err:
    if (rc != EXIT_SUCCESS) { 
        session->SendNok(PKT_PLAYER_REVIVE, errMsg);
    } else {
        K_LOG_TRACE( "Player Revive End");
        session->SendOk(PKT_PLAYER_REVIVE);
    }
}
