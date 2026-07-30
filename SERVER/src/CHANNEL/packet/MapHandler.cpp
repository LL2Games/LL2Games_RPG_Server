#include "common.h"
#include "MapHandler.h"
#include "ChannelSession.h"
#include "PacketParser.h"
#include "K_slog.h"
#include "utility.h"
#include <sstream>
#include <stdexcept>


void MapHandler::Execute(PacketContext * ctx)
{
    ChannelSession *session = nullptr;
    int rc = EXIT_SUCCESS;
    int mapId = 0;
    int playerid = 0;

    size_t offset = 0;
    std::string errMsg;
    std::string player_id;
    std::string map_id;


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

    
   // playerID 
    if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        player_id,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    K_LOG_TRACE( "PlayerID [%s]", player_id.c_str());

    // mapID 
     if(!PacketParser::ParseLengthPrefixedString(
        ctx->payload,
        ctx->payload_len,
        offset,
        map_id,
        errMsg
    ))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    if(!utility::StringToInt(player_id, playerid))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "playerid String To Int Fail");
        goto err;
    }

    if(!utility::StringToInt(map_id, mapId))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "mapId String To Int Fail");
        goto err;
    }

    if(!ctx->map_service)
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "map_service is fail");
        errMsg = "map_service is NULL";
        goto err;
    }

    rc = ctx->map_service->EnterMap(playerid, mapId);

    if(rc != EXIT_SUCCESS)
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "EnterMap fail");
        errMsg = "EnterMap Failed";
        goto err;
    }
   
err:
    if (rc != EXIT_SUCCESS) {
        session->SendNok(PKT_ENTER_MAP, errMsg);
    } else {
        K_LOG_TRACE( "MAP HANDLER END");
        session->SendOk(PKT_ENTER_MAP);
    }
}

