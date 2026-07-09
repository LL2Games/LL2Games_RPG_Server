#include "Packet.h"
#include "LoginHandler.h"
#include "Client.h"
#include "MySQLManager.h"
#include <sys/socket.h>
#include "K_slog.h"
#include "PacketParser.h"

void LoginHandler::Execute(PacketContext* ctx)
{
    int rc = EXIT_SUCCESS;
    std::string errMsg;
    size_t offset = 0;
    Client* client = nullptr;
    std::string id;
    std::string pw;

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
    
    K_LOG_DEBUG( "id=%s pw=%s\n", id.c_str(), pw.c_str());

    if (!MySQLManager::GetInstance()->Login(id, pw))
    {
        K_LOG_ERROR( "Login fail invalid ID/PW\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]Login fail invalid ID/PW";
        goto err;
    }

err:
    if (rc != EXIT_SUCCESS)
    {
        K_LOG_ERROR( "Err=%s", errMsg.c_str());
        client->SendNok(PKT_LOGIN, errMsg);
    }
    else
    {
        K_LOG_TRACE( "Login SUCCESS ID=%s", id.c_str());
        client->SendOk(PKT_LOGIN);
    }
}
