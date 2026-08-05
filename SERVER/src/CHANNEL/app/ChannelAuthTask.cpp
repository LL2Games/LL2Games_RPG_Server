#include "ChannelAuthTask.h"
#include "ChannelServer.h"
#include "ChannelAuthResult.h"
#include "RedisConnectionPool.h"
#include "PacketParser.h"
#include "RedisClient.h"
#include "AuthTicketService.h"
#include "K_slog.h"
#include <cstring>
#include <stdexcept>




namespace
{
std::string ReadChannelTicketFromPayload(const std::string& payload)
{
    std::size_t offset = 0;
    std::string ticket;
    std::string parseError;

    if (!PacketParser::ParseLengthPrefixedString(
            payload.data(),
            payload.size(),
            offset,
            ticket,
            parseError))
    {
        throw std::runtime_error("invalid channel authentication payload");
    }

    if (offset != payload.size())
    {
        throw std::runtime_error("trailing channel authentication payload");
    }

    return ticket;
}
}

ChannelAuthTask::ChannelAuthTask(ChannelServer* server, int fd, uint64_t sessionId, uint64_t generation, std::string payload)
        : m_server(server),
        m_fd(fd),
        m_sessionId(sessionId),
        m_generation(generation),
        m_payload(std::move(payload))
{

}

void ChannelAuthTask::Execute()
{
    ChannelAuthResult result;
    result.fd = m_fd;
    result.sessionId = m_sessionId;
    result.generation = m_generation;
    result.success = false;

    if (m_server == nullptr)
    {
        K_LOG_ERROR("ChannelAuthTask failed: server is null");
        return;
    }

    try
    {
        const std::string channelTicket =ReadChannelTicketFromPayload(m_payload);
        RedisConnectionGuard redisGuard(m_server->GetRedisConnectionPool());

        if (!redisGuard)
        {
            result.error ="redis connection acquire failed";
            m_server->PushAuthResult(std::move(result));
            return;
        }

        const auto claims = AuthTicketService::ConsumeChannelTicket(*redisGuard.Get(),channelTicket);

        if (!claims.has_value())
        {
            result.error ="invalid or expired channel ticket";
            m_server->PushAuthResult(std::move(result));
            return;
        }

        if (!claims->MatchesChannel(m_server->GetChannelId()))
        {
            K_LOG_ERROR("Channel authentication failed: Channel ID mismatch");
            K_LOG_ERROR("ChannelAuthTask failed: channel target mismatch");
            result.error = "channel authentication failed";
            m_server->PushAuthResult(std::move(result));
            return;
        }

        const int characterId =claims->characterId;
        result.characterId = characterId;

        std::unique_ptr<Player> player = m_server->GetPlayerService()->LoadPlayer(characterId,redisGuard.Get());

        if (!player)
        {
            result.error = "player load failed";
            m_server->PushAuthResult(std::move(result));
            return;
        }

        if (!PlayerService::LoadInventoryMeta(player.get()) || !PlayerService::LoadInventory(player.get()))
        {
            result.error ="player inventory load failed";
            m_server->PushAuthResult(std::move(result));
            return;
        }

        if (!PlayerService::LoadLearnedSkill(player.get()))
        {
            K_LOG_ERROR("ChannelAuthTask: learned skill load failed");
        }

        if (!PlayerService::LoadSlotSetting(player.get()))
        {
            K_LOG_ERROR("ChannelAuthTask: quick slot load failed");
            result.error = "player quick slot load failed";
            m_server->PushAuthResult(std::move(result));
        
            return;
        }

        result.success = true;
        result.player = std::move(player);
        m_server->PushAuthResult(std::move(result));
    }
    catch (const std::exception& exception)
    {
        result.error ="invalid channel authentication payload";
        K_LOG_ERROR("ChannelAuthTask exception. fd:%d error:%s",m_fd,exception.what());
        m_server->PushAuthResult(std::move(result));
    }
}
