#include "CharacterHandler.h"
#include "CharacterService.h"
#include "WorldSession.h"
#include "K_slog.h"
#include "PacketParser.h"
#include "RedisConnectionPool.h"

void CharacterHandler::Execute(PacketContext* ctx)
{
    WorldSession *session = nullptr;
    CharacterService *char_service = nullptr;
    int rc = EXIT_SUCCESS;
    std::string errMsg;

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
    

err:
    if (rc != EXIT_SUCCESS)
    {
        session->SendNok(PKT_SELECT_CHARACTER, errMsg);
    }
    else
    {   
        RedisConnectionGuard redisGuard(ctx->redis_pool);
        if (!redisGuard)
        {
            session->SendNok(PKT_SELECT_CHARACTER, "redis connection acquire failed");
            return;
        }
        std::vector<std::string> char_list = char_service->GetCharacterList(session->GetID(), *redisGuard.Get());
        session->SendOk(PKT_SELECT_CHARACTER, char_list);
    }
}
