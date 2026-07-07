#include "PacketProcessTask.h"

#include "ChannelServer.h"
#include "ChannelSession.h"
#include "IPacketHandler.h"
#include "K_slog.h"

#include <memory>
#include <utility>
#include <exception>

PacketProcessTask::PacketProcessTask(ChannelServer* server, ChannelSession* session, int fd, uint64_t sessionId, uint64_t generation, uint16_t type, std::string payload)
    : m_server(server),
      m_session(session),
      m_fd(fd),
      m_sessionId(sessionId),
      m_generation(generation),
      m_type(type),
      m_payload(std::move(payload))
{
}
void PacketProcessTask::Execute()
{
    if (m_server == nullptr)
    return;

    ChannelSession* session = m_server->BeginValidSessionTask(m_fd, m_sessionId, m_generation);

    if (session == nullptr)
        return;

    struct SessionTaskGuard
    {
        ChannelServer* server;
        ChannelSession* session;
        ~SessionTaskGuard()
        {
            if (server != nullptr && session != nullptr)
                server->EndSessionTask(session);
        }
    } taskGuard{m_server, session};

    try
    {
        auto handler = m_factory.Create(m_type);
        if (!handler)
            return;

        PacketContext ctx;
        ctx.type = m_type;
        ctx.channel_session = session;
        ctx.fd = m_fd;
        ctx.payload = const_cast<char*>(m_payload.c_str());
        ctx.payload_len = static_cast<int>(m_payload.size());

        ctx.player_manager = m_server->GetPlayerManager();
        ctx.map_service = m_server->GetMapService();
        ctx.player_service = m_server->GetPlayerService();
        ctx.stat_service = m_server->GetStatService();
        ctx.item_service = m_server->GetItemService();
        ctx.combat_service = m_server->GetCombatService();
        ctx.trade_service = m_server->GetTradeService();

        handler->Execute(&ctx);
    }
    catch (const std::exception& e)
    {
        K_slog_trace(
            K_SLOG_ERROR,
            "[PacketProcessTask] exception fd:%d type:%d error:%s",
            m_fd,
            m_type,
            e.what()
        );
    }
    catch (...)
    {
        K_slog_trace(
            K_SLOG_ERROR,
            "[PacketProcessTask] unknown exception fd:%d type:%d",
            m_fd,
            m_type
        );
    }
}
