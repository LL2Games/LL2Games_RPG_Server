#include "ChannelSession.h"
#include "ChannelServer.h"
#include "PacketParser.h"
#include "IPacketFactory.h"
#include "Packet.h"
#include "MapInstance.h"
#include "PlayerManager.h"
#include "ChannelAuthTask.h"
#include "PacketProcessTask.h"
#include "GameplayGate.h"

#include <openssl/err.h>

ChannelSession::ChannelSession(int fd, ChannelServer* server, uint64_t sessionId, uint64_t generation)
    : m_fd(fd),
      m_server(server),
      m_player(nullptr),
      m_playerManager(nullptr),
      m_sessionId(sessionId),
      m_generation(generation)
{
    m_recvBuf.reserve(8192);
}

ChannelSession::~ChannelSession()
{
    Gameplay::Guard guard(Gameplay::gate);
    if (m_player != nullptr)
    {
        m_player->SetSession(nullptr);

        if (m_server != nullptr)
        {
            std::string errMsg;
            int saveResult = m_server->GetStatService()->SaveRuntimeStat(*m_player, errMsg);
            if (saveResult != 0)
            {
                K_LOG_ERROR("SaveRuntimeStat failed. playerId[%d] err[%s]", m_player->GetId(), errMsg.c_str());
            }
        }

        MapInstance *map = m_player->GetCurrentMap();
        
        if (map != nullptr)
        {
            //플레이어 접속 종료시 해당맵에서 퇴장
            map->OnLeave(m_player->GetId());
            K_LOG_DEBUG( "map->OnLeave(Id:%d)", m_player->GetId());
        }

        if (m_playerManager != nullptr)
        {
            K_LOG_TRACE( "[ChannelSession Destroy] RemovePlayer id:%d", m_player->GetId());
            m_playerManager->RemovePlayer(m_player->GetId());
        }
        else
        {
            K_LOG_ERROR( "[ChannelSession Destroy] playerManager is null. player id:%d", m_player->GetId());
        }
    }
}


bool ChannelSession::OnBytes(const uint8_t* data, size_t len)
{
    m_recvBuf.insert(m_recvBuf.end(), data, data+len);

    while (true)
    {
        ParseResult result = PacketParser::TryParse(m_recvBuf);
        if (result.status == ParseStatus::NeedMoreData)
        {
            return true;
        }

        if (result.status == ParseStatus::InvalidPacket)
        {
            K_LOG_ERROR( "Invalid packet");
            return false;
        }

        Dispatch(result.packet);
    }
    return true;
}

void ChannelSession::Dispatch(const ParsedPacket &pkt)
{
     if (m_server == nullptr)
    {
        return;
    }

    if (pkt.type == PKT_CHANNEL_AUTH)
    {
        if (!TryBeginAuthentication())
        {
            SendNok(PKT_CHANNEL_AUTH,"Channel authentication is already in progress or completed");
            return;
        }

        auto task = std::make_unique<ChannelAuthTask>(m_server, m_fd, m_sessionId, m_generation, pkt.payload);
        m_server->GetAuthThreadPool()->Submit(std::move(task));
        return;
    }

    if (!IsAuthenticated())
    {
        K_LOG_ERROR("Unauthenticated channel packet rejected. fd:%d type:%u",m_fd, static_cast<unsigned int>(pkt.type));

        SendNok(pkt.type,"Channel authentication required");
        return;
    }

    auto task = std::make_unique<PacketProcessTask>(
            m_server,
            this,
            m_fd,
            m_sessionId,
            m_generation,
            pkt.type,
            pkt.payload
        );

    m_server->GetThreadPool()->SubmitByKey(m_sessionId,std::move(task));
}
// 지금 방식은 클라이언트 하나에 해당해서 Send를 하는 방식인데 Player 클래스를 vector로 가지고 있고
// 같은 맵, 시야 범위 등등 환경요소들을 확인해서 보내는 방식으로 변경 필요

//[L][V] [L][V] [L][V]

//클라입력 $  [L][V]

int ChannelSession::Send(int type, const std::vector<std::string>& payload)
{

    std::string body = PacketParser::MakeBody(payload);
    std::string packet = PacketParser::MakePacket(type, body);
    
    return EnqueueSend(std::move(packet));
}


int ChannelSession::SendOk(int type, std::vector<std::string> payload)
{
    std::vector<std::string>::iterator payloadIter;

    payload.insert(payload.begin(), "ok");
    std::string body = PacketParser::MakeBody(payload);
    std::string packet = PacketParser::MakePacket(type, body);

   return EnqueueSend(std::move(packet));
}

int ChannelSession::SendNok(int type, const std::string &errMsg, std::vector<std::string> payload)
{
    payload.insert(payload.begin(),errMsg);
    payload.insert(payload.begin(),"nok");
    std::string body = PacketParser::MakeBody(payload);
    std::string packet = PacketParser::MakePacket(type, body);

    return EnqueueSend(std::move(packet));
}

int ChannelSession::EnqueueSend(std::string packet)
{
     if (packet.empty())
        return -1;

    if (m_closing.load())
        return -1;

    bool needEnableWrite = false;

    {
        std::lock_guard<std::mutex> lock(m_sendMutex);

        needEnableWrite = m_sendQueue.empty();
        m_sendQueue.push_back(std::move(packet));
        m_pendingSendBytes += m_sendQueue.back().size();

    }

    if (needEnableWrite && m_server != nullptr)
    {
       //K_LOG_TRACE( "[EnqueueSend] EnableWriteEvent fd:%d", m_fd);
        m_server->EnableWriteEvent(m_fd);
    }

    return 0;
}

bool ChannelSession::FlushSend()
{
    if (m_closing.load())
        return false;

    std::lock_guard<std::mutex> lock(m_sendMutex);

    while (!m_sendQueue.empty())
    {
        std::string& packet = m_sendQueue.front();
        ssize_t sent = send(
            m_fd,
            packet.data() + m_sendOffset,
            packet.size() - m_sendOffset,
            MSG_NOSIGNAL
        );
        if (sent > 0)
        {
            m_sendOffset += static_cast<size_t>(sent);

            if (m_sendOffset == packet.size())
            {
                m_pendingSendBytes -= packet.size();
                m_sendQueue.pop_front();
                m_sendOffset = 0;
            }

            continue;
        }

        if (sent < 0)
        {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return true;

            return false;
        }

        return false;
    }

    return true;
}

bool ChannelSession::HasPendingSend() const
{
   std::lock_guard<std::mutex> lock(m_sendMutex);
   return !m_sendQueue.empty();
}

bool ChannelSession::TryBeginTask()
{
    std::lock_guard<std::mutex> lock(m_taskMutex);
    if (m_closing.load())
        return false;

    ++m_inFlightTasks;
    return true;
}

void ChannelSession::EndTask()
{
    std::lock_guard<std::mutex> lock(m_taskMutex);
    if (m_inFlightTasks > 0)
        --m_inFlightTasks;

    if (m_inFlightTasks == 0)
        m_taskCv.notify_all();
}

void ChannelSession::MarkClosing()
{
    m_closing.store(true);
    std::lock_guard<std::mutex> lock(m_taskMutex);
    if (m_inFlightTasks == 0)
        m_taskCv.notify_all();
}

void ChannelSession::WaitForNoTasks()
{
    std::unique_lock<std::mutex> lock(m_taskMutex);
    m_taskCv.wait(lock, [this]() {
        return m_inFlightTasks == 0;
    });
}

bool ChannelSession::TryBeginAuthentication()
{
    ChannelAuthenticationState expected = ChannelAuthenticationState::Unauthenticated;
    return m_authenticationState.compare_exchange_strong(expected,ChannelAuthenticationState::Authenticating);
}

void ChannelSession::MarkAuthenticated()
{
     m_authenticationState.store(ChannelAuthenticationState::Authenticated);
}

void ChannelSession::ResetAuthentication()
{
     m_authenticationState.store(ChannelAuthenticationState::Unauthenticated);
}

bool ChannelSession::IsAuthenticated() const
{
    return m_authenticationState.load() == ChannelAuthenticationState::Authenticated;
}

void ChannelSession::FinalizePlayer()
{
    Gameplay::Guard guard(Gameplay::gate);

    if (m_player == nullptr)
    {
        return;
    }

    Player* player = m_player;
    player->SetSession(nullptr);

    MapInstance* map = player->GetCurrentMap();

    if (map != nullptr)
    {
        map->OnLeave(player->GetId());
        K_LOG_DEBUG("map->OnLeave(Id:%d)",player->GetId());
    }

    if (m_server != nullptr && player->IsSaveNeeded())
    {
        PlayerSaveData saveData = player->MakeSaveData();

        if (!m_server->SubmitFinalPlayerDataSave(std::move(saveData)))
        {
            K_LOG_ERROR("Final player data save submission failed. playerId[%d]",player->GetId());
        }
    }

    const int playerId = player->GetId();

    // RemovePlayer에서 Player 객체가 삭제될 수 있으므로 이후 player를 사용하면 안 된다.
    m_player = nullptr;

    if (m_playerManager != nullptr)
    {
        m_playerManager->RemovePlayer(playerId);
        K_LOG_TRACE("[ChannelSession Finalize] RemovePlayer id:%d",playerId);
    }
    else
    {
        K_LOG_ERROR("[ChannelSession Finalize] playerManager is null. player id:%d",playerId);
    }
}

bool ChannelSession::AttachTls(SSL_CTX* context)
{
    m_tls.reset(SSL_new(context));

    if(!m_tls || SSL_set_fd(m_tls.get(), m_fd) != 1)
    {
        m_tls.reset();
        return false;
    }

    return true;
}

ChannelSession::TlsHandshakeState ChannelSession::AdvanceTlsHandshake()
{
    ERR_clear_error();

    const int result = SSL_accept(m_tls.get());

    if(result == 1)
    {
        m_tlsHandshakeState = TlsHandshakeState::Ready;
        return m_tlsHandshakeState;
    }

    const int sslError = SSL_get_error(m_tls.get(), result);
    switch(sslError)
    {
        case SSL_ERROR_WANT_READ:
            m_tlsHandshakeState = TlsHandshakeState::WantRead;
            break;
        case SSL_ERROR_WANT_WRITE:
            m_tlsHandshakeState = TlsHandshakeState::WantWrite;
            break;
        default:
            K_LOG_ERROR("[Channel] TLS handshake failed fd:%d sslError:%d", m_fd, sslError);
            m_tlsHandshakeState = TlsHandshakeState::Failed;
            break;
    }

    return m_tlsHandshakeState;
}

ChannelSession::TlsReadResult ChannelSession::ReadTls(char* buffer, std::size_t capacity, std::size_t& bytesRead)
{
    bytesRead = 0;
    ERR_clear_error();

    const int result = SSL_read_ex(m_tls.get(), buffer, capacity, &bytesRead);
    if (result == 1)
        return TlsReadResult::Data;

    switch (SSL_get_error(m_tls.get(), result))
    {
    case SSL_ERROR_WANT_READ:
        return TlsReadResult::WantRead;
    case SSL_ERROR_WANT_WRITE:
        return TlsReadResult::WantWrite;
    case SSL_ERROR_ZERO_RETURN:
        return TlsReadResult::Closed;
    default:
        return TlsReadResult::Failed;
    }
}

ChannelSession::TlsWriteResult ChannelSession::FlushTlsWrites()
{
    if (IsClosing() || GetTlsHandshakeState() != TlsHandshakeState::Ready)
        return TlsWriteResult::Failed;

    std::lock_guard<std::mutex> lock(m_sendMutex);

    constexpr std::size_t maxPacketsPerFlush = 32;
    constexpr std::size_t maxBytesPerFlush = 64 * 1024;
    std::size_t bytesSent = 0;
    for (std::size_t sent = 0; sent < maxPacketsPerFlush && bytesSent < maxBytesPerFlush && !m_sendQueue.empty(); ++sent)
    {
        const std::string& packet = m_sendQueue.front();
        std::size_t bytesWritten = 0;
        ERR_clear_error();

        const int result = SSL_write_ex(
            m_tls.get(), packet.data(), packet.size(), &bytesWritten);

        if (result == 1)
        {
            bytesSent += bytesWritten;
            if (result == 1)
            {
                bytesSent += bytesWritten;
                m_pendingSendBytes -= packet.size();
                m_sendQueue.pop_front();
                continue;
            }
            m_sendQueue.pop_front();
            continue;
        }

        switch (SSL_get_error(m_tls.get(), result))
        {
        case SSL_ERROR_WANT_READ:
            return TlsWriteResult::WantRead;
        case SSL_ERROR_WANT_WRITE:
            return TlsWriteResult::WantWrite;
        default:
            return TlsWriteResult::Failed;
        }
    }

    return m_sendQueue.empty() ? TlsWriteResult::Drained : TlsWriteResult::Pending;
}