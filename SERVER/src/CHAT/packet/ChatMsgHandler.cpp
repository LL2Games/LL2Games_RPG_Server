#include "ChatMsgHandler.h"
#include "Packet.h"
#include "Client.h"
#include <functional>
#include <string>
#include "K_slog.h"
#include "PacketParser.h"
#include "CommandDispatcher.h"

void ChatMsgHandler::Execute(PacketContext *ctx)
{
    int rc = EXIT_SUCCESS;
    std::string errMsg;
    size_t offset = 0;
    Client *client = nullptr;
    std::vector<Client *> clients = *ctx->clients;
    std::function<void(const std::string &, const std::string &, const int)> &broadcast = ctx->broadcast;
    std::string msg;
    CommandDispatcher *dispatcher = nullptr;

    client = ctx->client;
    if (client == nullptr)
    {
        K_LOG_ERROR( "client is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]client is nullptr";
        goto err;
    }

    dispatcher = ctx->dispatcher;
    if (dispatcher == nullptr)
    {
        K_LOG_ERROR( "dispatcher is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]dispatcher is nullptr";
        goto err;
    }

    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            msg,
            errMsg))
    {
        rc = EXIT_FAILURE;
        goto err;
    }

    K_LOG_TRACE( "[ChatMsg: %d] %s", client->GetFD(), msg.c_str());

#if 1 /* gunoo22 260127 channelD로 메시지 보내는 경우 */
    if (dispatcher->Dispatch(msg)) //커맨드인경우
    {
        //커맨드 응답? 등 수행
    }
    else    //일반채팅인경우
    {
    }
/*
/교환신청
/파티초대
등의 메시지 들어올시 channelD로 messageQueue형식으로 전송하기
*/
#endif

err:
    if (rc != EXIT_SUCCESS)
    {
        client->SendNok(PKT_CHAT, errMsg);
    }
    else
    {
        broadcast(client->GetNick(), msg, client->GetFD());
        // temp
        for (auto c : clients)
        {
            c->GetFD();
        }
        std::vector<std::string> chatMsg;
        chatMsg.push_back("msg=" + msg);
        client->SendOk(PKT_CHAT, chatMsg);
    }
}