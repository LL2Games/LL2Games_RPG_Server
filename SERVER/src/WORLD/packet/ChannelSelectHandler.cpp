#include "ChannelSelectHandler.h"
#include "WorldSession.h"
#include "ChannelManager.h"
#include "K_slog.h"
#include "PacketParser.h"
#include "RedisConnectionPool.h"
#include "AuthTicketService.h"
#include "CharacterService.h"
#include "RedisCommonEnum.h"

void ChannelSelectHandler::Execute(PacketContext* ctx)
{
    if (ctx == nullptr)
    {
        K_LOG_ERROR("ChannelSelectHandler failed: context is null");
        return;
    }

    WorldSession* session = ctx->world_session;

    if (session == nullptr)
    {
        K_LOG_ERROR("ChannelSelectHandler failed: session is null");
        return;
    }

    if (!session->IsAuthenticated())
    {
        session->SendNok(PKT_SELECT_CHANNEL,"World authentication required");
        return;
    }

    CharacterService* characterService = ctx->char_service;

    ChannelManager* channelManager = ctx->channel_manager;

    if (characterService == nullptr ||channelManager == nullptr || ctx->redis_pool == nullptr)
    {
        K_LOG_ERROR("ChannelSelectHandler failed: service is null");
        session->SendNok(PKT_SELECT_CHANNEL,"Channel selection failed");
        return;
    }

    std::size_t offset = 0;
    std::string parseError;
    int characterId = 0;
    int channelId = 0;

    if (!PacketParser::ParseNextIntField(
            ctx->payload,
            static_cast<std::size_t>(ctx->payload_len),
            offset,
            characterId,
            parseError))
    {
        session->SendNok(PKT_SELECT_CHANNEL,"Invalid character ID");
        return;
    }

    if (!PacketParser::ParseNextIntField(
            ctx->payload,
            static_cast<std::size_t>(ctx->payload_len),
            offset,
            channelId,
            parseError))
    {
        session->SendNok(PKT_SELECT_CHANNEL,"Invalid channel ID");
        return;
    }

    if (offset != static_cast<std::size_t>(ctx->payload_len))
    {
        session->SendNok(PKT_SELECT_CHANNEL,"Invalid channel selection payload");
        return;
    }

    const std::string accountId = session->GetID();

    if (!characterService->OwnsCharacter(accountId,characterId))
    {
        K_LOG_ERROR("ChannelSelectHandler failed: character ownership rejected");
        session->SendNok(PKT_SELECT_CHANNEL,"Character ownership verification failed");
        return;
    }
    const std::string channelIdString = std::to_string(channelId);
    const auto selectedChannel = channelManager->SelectChannel(channelIdString);

    if (!selectedChannel.has_value())
    {
        session->SendNok(PKT_SELECT_CHANNEL,"Invalid channel");
        return;
    }

    RedisConnectionGuard redisGuard(ctx->redis_pool);

    if (!redisGuard)
    {
        session->SendNok(PKT_SELECT_CHANNEL,"Redis connection acquire failed");
        return;
    }

    const int channelState = channelManager->CanEnterChannel(channelIdString,*redisGuard.Get());

    if (channelState == static_cast<int>(E_ChannelState::Full) ||
        channelState == static_cast<int>(E_ChannelState::Die))
    {
        session->SendNok(PKT_SELECT_CHANNEL,"Channel is not available");
        return;
    }

    ChannelTicketClaims claims;
    claims.accountId = accountId;
    claims.characterId = characterId;
    claims.channelId = channelId;

    const auto channelTicket = AuthTicketService::IssueChannelTicket(*redisGuard.Get(),claims);

    if (!channelTicket.has_value())
    {
        session->SendNok(PKT_SELECT_CHANNEL,"Channel ticket creation failed");
        return;
    }

    const ChannelInfo& channelInfo = *selectedChannel;

    K_LOG_TRACE("[FLOW][WORLD] channel ticket issued. accountId[%s] characterId[%d] channelId[%d]", accountId.c_str(), characterId, channelId);
    
    session->SendOk(PKT_SELECT_CHANNEL,
        {
            channelInfo.ip,
            std::to_string(channelInfo.port),
            std::to_string(channelState),
            *channelTicket
        }
    );
}
