#include "ChannelSelectHandler.h"
#include "WorldSession.h"
#include "ChannelManager.h"
#include "K_slog.h"
#include "PacketParser.h"
#include "RedisConnectionPool.h"

void ChannelSelectHandler::Execute(PacketContext* ctx)
{
    WorldSession *session = nullptr;
    ChannelManager *channel_manager = nullptr;
    int rc = EXIT_SUCCESS;
    std::string errMsg;
    size_t offset = 0;
    std::string channel_id;

    if (ctx == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] ctx is nullptr\n", __FUNCTION__, __LINE__);
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]ctx is nullptr";
        goto err;
    }
    session = ctx->world_session;
    if (session == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] session is nullptr\n", __FUNCTION__, __LINE__);
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]session is nullptr";
        goto err;
    }
    channel_manager = ctx->channel_manager;
    if (channel_manager == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] channel_manager is nullptr\n", __FUNCTION__, __LINE__);
        rc = EXIT_FAILURE;
        errMsg = "[" + std::to_string(rc) + "]channel_manager is nullptr";
        goto err;
    }
    
    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            channel_id,
            errMsg))
    {
        rc = EXIT_FAILURE;
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] ParseLengthPrefixedString fail", __FUNCTION__, __LINE__);
        goto err;
    }

    K_slog_trace(K_SLOG_DEBUG,
        "[%s] client(%d) channel_id=[%s]",
        __FUNCTION__, session->GetFD(), channel_id.c_str());

err:
    if (rc != EXIT_SUCCESS)
    {
        session->SendNok(PKT_SELECT_CHANNEL, errMsg);
    }
    else
    {
        auto opt = channel_manager->SelectChannel(channel_id);
        if (!opt)
        {
            errMsg = std::string("channel(" + channel_id + ") is invalid");
            session->SendNok(PKT_SELECT_CHANNEL, errMsg);
        }
        else
        {
            ChannelInfo info = *opt;
            RedisConnectionGuard redisGuard(ctx->redis_pool);
            if (!redisGuard)
            {
                session->SendNok(PKT_SELECT_CHANNEL, "redis connection acquire failed");
                return;
            }
            
            int channel_state = channel_manager->CanEnterChannel(channel_id, *redisGuard.Get());
            // switch (channel_state)
            // {
            // case ChannelState::E_Normal: //정상
            //     break;
            // case ChannelState::E_Busy: //혼잡
            //     break;
            // case ChannelState::E_Full:  //만원
            //     break;
            // case ChannelState::E_Die:   //죽음
            //     break;
            // }

            std::vector<std::string> channel_info;
            channel_info.push_back(info.ip);
            channel_info.push_back(std::to_string(info.port));
            channel_info.push_back(std::to_string(channel_state)); //상태에 따른 클라이언트 처리 가능
            session->SendOk(PKT_SELECT_CHANNEL, channel_info);
        }
    }
}
