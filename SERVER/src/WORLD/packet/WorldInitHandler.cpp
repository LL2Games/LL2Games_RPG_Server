#include "WorldInitHandler.h"
#include "K_slog.h"
#include "WorldSession.h"
#include "PacketParser.h"

void WorldInitHandler::Execute(PacketContext *ctx)
{
    WorldSession *session = nullptr;
    size_t offset = 0;
    std::string account_id;
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

    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            account_id,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    K_LOG_DEBUG( "client(%d) account_id=[%s]", session->GetFD(), account_id.c_str());

    session->SetAccountid(account_id);

err:
    if (rc != EXIT_SUCCESS)
    {
        session->SendNok(PKT_INIT_WORLD, errMsg);
    }
    else
        session->SendOk(PKT_INIT_WORLD);
}