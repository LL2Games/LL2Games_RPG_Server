#include "PlayerSaveTask.h"

#include "ChannelServer.h"
#include "ChannelSession.h"
#include "Player.h"
#include "PlayerDataSaveService.h"
#include "K_slog.h"

#include <exception>
#include <string>

PlayerSaveTask::PlayerSaveTask(ChannelServer* server,const int fd,const std::uint64_t sessionId,const std::uint64_t generation)
    : m_server(server),
      m_fd(fd),
      m_sessionId(sessionId),
      m_generation(generation)
{
}

void PlayerSaveTask::Execute()
{
    if (m_server == nullptr)
    {
        return;
    }

    ChannelSession* session = m_server->BeginValidSessionTask(m_fd,m_sessionId,m_generation);

    if (session == nullptr)
    {
        return;
    }

    struct SessionTaskGuard
    {
        ChannelServer* server = nullptr;
        ChannelSession* session = nullptr;

        ~SessionTaskGuard()
        {
            if (server != nullptr && session != nullptr)
            {
                server->EndSessionTask(session);
            }
        }
    } taskGuard{m_server, session};

    try
    {
        Player* player = session->GetPlayer();

        if (player == nullptr)
        {
            return;
        }

        std::string errMsg;

        if (!m_server->GetPlayerDataSaveService()->SaveIfNeeded(*player, errMsg))
        {
            K_LOG_ERROR("[PlayerSaveTask] save failed. ""playerId[%d] error[%s]",player->GetId(),errMsg.c_str());
        }
    }
    catch (const std::exception& exception)
    {
        K_LOG_ERROR("[PlayerSaveTask] exception. ""fd[%d] error[%s]",m_fd,exception.what());
    }
    catch (...)
    {
        K_LOG_ERROR("[PlayerSaveTask] unknown exception. fd[%d]",m_fd);
    }
}