#include "CheckDupNickHandler.h"
#include "CharacterService.h"
#include "WorldSession.h"
#include "K_slog.h"
#include "PacketParser.h"
#include "RedisConnectionPool.h"

void CheckDupNickHandler::Execute(PacketContext* ctx)
{
    WorldSession *session = nullptr;
    CharacterService *char_service = nullptr;
    int rc = EXIT_SUCCESS;
    std::string errMsg;
    size_t offset = 0;
    std::string nick;

    if (ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }
    session = ctx->world_session;
    if (session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }
    char_service = ctx->char_service;
    if (char_service == nullptr)
    {
        K_LOG_ERROR( "char_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]char_service is nullptr";
        goto err;
    }

    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            nick,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    rc = char_service->CheckDupNick(nick);
    
err:
    if (rc != EXIT_SUCCESS)
    {
        session->SendNok(PKT_CHECK_DUP_CHAR, errMsg);
    }
    else
    {   
        session->SendOk(PKT_CHECK_DUP_CHAR);
    }
}
