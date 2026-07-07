#include "ChannelAuthTask.h"
#include "ChannelServer.h"
#include "ChannelAuthResult.h"
#include "RedisConnectionPool.h"
#include "PacketParser.h"
#include "RedisClient.h"
#include "K_slog.h"
#include <cstring>
#include <stdexcept>

namespace
{
int ReadCharacterIdFromPayload(const std::string& payload)
{
    if (payload.size() < sizeof(uint16_t))
    {
        throw std::runtime_error("invalid auth payload");
    }

    uint16_t len = 0;
    std::memcpy(&len, payload.data(), sizeof(uint16_t));

    if (payload.size() < sizeof(uint16_t) + len)
    {
        throw std::runtime_error("invalid auth payload length");
    }

    std::string value(payload.data() + sizeof(uint16_t), len);
    return std::stoi(value);
}
}

ChannelAuthTask::ChannelAuthTask(ChannelServer* server, int fd, std::string payload)
    : m_server(server),
      m_fd(fd),
      m_payload(std::move(payload))
{
}

void ChannelAuthTask::Execute()
{

    ChannelAuthResult result;
    result.fd = m_fd;
    result.success = false;

    try
    {
        int characterId = ReadCharacterIdFromPayload(m_payload);
        result.characterId = characterId;

        RedisConnectionGuard redisGuard(m_server->GetRedisConnectionPool());
    
        if (!redisGuard)
        {
            result.error = "redis connection acquire failed";
            m_server->PushAuthResult(std::move(result));
            return;
        }

        std::unique_ptr<Player> player = m_server->GetPlayerService()->LoadPlayer(characterId, redisGuard.Get());

        if (!player)
        {
            result.error = "player load failed";
            m_server->PushAuthResult(std::move(result));
            return;
        }

        if (!PlayerService::LoadInventoryMeta(player.get()) ||!PlayerService::LoadInventory(player.get()))
        {
            result.error = "player inventory load failed";
            m_server->PushAuthResult(std::move(result));
            return;
        }

        if (!PlayerService::LoadLearnedSkill(player.get()))
        {
            K_slog_trace(K_SLOG_ERROR, "[ChannelAuthTask] LoadLearnedSkill failed. characterId:%d", characterId);
        }

        result.success = true;
        result.player = std::move(player);
        m_server->PushAuthResult(std::move(result));
    }
    catch (const std::exception& e)
    {
        result.error = e.what();
        K_slog_trace(K_SLOG_ERROR, "[ChannelAuthTask] exception fd:%d error:%s", m_fd, result.error.c_str());
        m_server->PushAuthResult(std::move(result));
    }
}
