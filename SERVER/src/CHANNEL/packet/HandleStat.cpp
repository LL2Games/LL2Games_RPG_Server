#include "PlayerHandler.h"
#include "ChannelSession.h"
#include "Player.h"
#include "PacketParser.h"
#include "CharacterStat.h"
#include "StatPacketFactory.h"
#include "StatService.h"

void PlayerHandler::HandleStatView(PacketContext* ctx)
{
    int rc = EXIT_SUCCESS;
    ChannelSession *session = nullptr;
    Player *player = nullptr;
    // size_t char_id;
    std::string errMsg;
    // size_t offset = 0;

    if (ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }
    session = ctx->channel_session;
    if (session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    player = session->GetPlayer();
    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }


err:
    if (rc != EXIT_SUCCESS)
    {
        session->SendNok(PKT_STAT_VIEW, errMsg);
    }
    else
    {
        CharacterStat stat = player->GetStatSnapShot();
        StatInfoPacket statPkt = StatPacketFactory::MakeStatInfo(stat);
        K_LOG_DEBUG( "str=%d\n", statPkt.str);

        std::vector<std::string> stat_info;
        stat_info.push_back(std::to_string(statPkt.str));
        stat_info.push_back(std::to_string(statPkt.dex));
        stat_info.push_back(std::to_string(statPkt.intel));
        stat_info.push_back(std::to_string(statPkt.luck));
        stat_info.push_back(std::to_string(statPkt.maxHp));
        stat_info.push_back(std::to_string(statPkt.maxMp));
        stat_info.push_back(std::to_string(statPkt.curHp));
        stat_info.push_back(std::to_string(statPkt.curMp));
        stat_info.push_back(std::to_string(statPkt.remainAp));

        session->SendOk(PKT_STAT_VIEW, stat_info);
    }
}

void PlayerHandler::HandleStatUp(PacketContext* ctx)
{
    int rc = EXIT_SUCCESS;
    ChannelSession *session = nullptr;
    Player *player = nullptr;
    // size_t char_id;
    std::string errMsg;
    size_t offset = 0;
    std::string stat;
    StatService* stat_service = nullptr;

    if (ctx == nullptr)
    {
        K_LOG_ERROR( "ctx is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }
    session = ctx->channel_session;
    if (session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }

    player = session->GetPlayer();
    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]player is nullptr";
        goto err;
    }

    stat_service = ctx->stat_service;
    if (stat_service == nullptr)
    {
        K_LOG_ERROR( "stat_service is nullptr\n");
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]stat_service is nullptr";
        goto err;
    }

    // char_id = player->GetId();
    // if (char_id == 0)
    // {
    //     K_LOG_ERROR( "char_id is invalid\n");
    //     rc = EXIT_FAILURE;
    //     errMsg = "[" + std::to_string(rc) + "]char_id is invalid";
    //     goto err;
    // }
    
    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            stat,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_LOG_ERROR( "ParseLengthPrefixedString fail");
        goto err;
    }

    rc = stat_service->UpStat(*player, stat, errMsg);
    
err:
    if (rc != EXIT_SUCCESS)
    {
        session->SendNok(PKT_STAT_UP, errMsg);
    }
    else
    {
        session->SendOk(PKT_STAT_UP);
    }
}