#include "Packet.h"
#include "LoginHandler.h"
#include "Client.h"
#include "MySQLManager.h"
#include <sys/socket.h>
#include "K_slog.h"
#include "PacketParser.h"
#include "AuthTicketService.h"
#include "RedisConnectionPool.h"

void LoginHandler::Execute(PacketContext* ctx)
{
    int rc = EXIT_SUCCESS;
    std::string errMsg;
    size_t offset = 0;
    Client* client = nullptr;
    std::string id;
    std::string pw;
    std::string worldTicket;

    if (ctx == nullptr)
    {   
        K_LOG_ERROR("LoginHandler failed: context is null");
        return;
    }

    client = ctx->client;

    if (client == nullptr)
    {
        K_LOG_ERROR( "client is nullptr\n");
        return;
    }

    if (ctx->redis_pool == nullptr)
    {
        rc = EXIT_FAILURE;
        errMsg = "Redis pool is null";
        goto err;
    }

    //id
    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            id,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }
    //pw
    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            pw,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }
    
    if (!MySQLManager::GetInstance()->Login(id, pw))
    {
        K_LOG_ERROR( "Login fail invalid ID/PW\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]Login fail invalid ID/PW";
        goto err;
    }

    //로그인 이후 티켓을 생성 및 저장 
    {
        RedisConnectionGuard redisGuard(ctx->redis_pool);

        if (!redisGuard)
        {
            rc = EXIT_FAILURE;
            errMsg = "Redis connection acquire failed";
            goto err;
        }

        const auto issuedTicket = AuthTicketService::IssueWorldTicket(*redisGuard.Get(), id);

        if (!issuedTicket.has_value())
        {
            rc = EXIT_FAILURE;
            errMsg = "World ticket creation failed";
            goto err;
        }

        worldTicket = *issuedTicket;
    }

err:
    if (rc != EXIT_SUCCESS)
    {
        K_LOG_ERROR( "Err=%s", errMsg.c_str());
        client->SendNok(PKT_LOGIN, errMsg);
    }
    else
    {
        K_LOG_TRACE("[FLOW][LOGIN] authentication succeeded and world ticket issued. accountId[%s]", id.c_str());
        client->SendOk(PKT_LOGIN, {worldTicket});
    }
}
