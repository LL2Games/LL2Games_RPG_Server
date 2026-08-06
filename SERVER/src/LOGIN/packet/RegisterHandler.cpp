#include "Packet.h"
#include "RegisterHandler.h"
#include "Client.h"
#include "MySQLManager.h"
#include <sys/socket.h>
#include "K_slog.h"
#include "PacketParser.h"

void RegisterHandler::Execute(PacketContext* ctx)
{
    int rc = EXIT_SUCCESS;
    std::string errMsg;
    size_t offset = 0;
    Client* client = nullptr;
    std::string id;
    std::string pw, pw_check;

    client = ctx->client;
    if (client == nullptr)
    {
        K_LOG_ERROR( "client is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]client is nullptr";
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

    //pw_check
    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            pw_check,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }
    
    K_LOG_DEBUG( "id=%s", id.c_str());
    K_LOG_DEBUG( "pw=%s, pw_check=%s\n", pw.c_str(), pw_check.c_str());

    if (!MySQLManager::GetInstance()->Register(id, pw, pw_check))
    {
        K_LOG_ERROR( "Register fail\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]Register fail";
        goto err;
    }

err:
    if (rc != EXIT_SUCCESS)
    {
        K_LOG_ERROR( "Err=%s", errMsg.c_str());
        client->SendNok(PKT_REGISTER, errMsg);
    }
    else
    {
        K_LOG_TRACE( "Register SUCCESS ID=%s", id.c_str());
        client->SendOk(PKT_REGISTER);
    }
}
