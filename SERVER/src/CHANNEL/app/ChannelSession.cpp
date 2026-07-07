#include "ChannelSession.h"
#include "ChannelServer.h"
#include "PacketParser.h"
#include "IPacketFactory.h"
#include "Packet.h"
#include "MapInstance.h"
#include "PlayerManager.h"
#include "ChannelAuthTask.h"
#include "PacketProcessTask.h"

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
    if (m_player != nullptr)
    {
        m_player->SetSession(nullptr);

        MapInstance *map = m_player->GetCurrentMap();
        
        if (map != nullptr)
        {
            //플레이어 접속 종료시 해당맵에서 퇴장
            map->OnLeave(m_player->GetId());
            K_slog_trace(K_SLOG_DEBUG, "[%s:%s][%d] map->OnLeave(Id:%d)", __FILE__, __FUNCTION__, __LINE__, m_player->GetId());
        }

        if (m_playerManager != nullptr)
        {
            K_slog_trace(K_SLOG_TRACE, "[ChannelSession Destroy] RemovePlayer id:%d", m_player->GetId());
            m_playerManager->RemovePlayer(m_player->GetId());
        }
        else
        {
            K_slog_trace(K_SLOG_ERROR, "[ChannelSession Destroy] playerManager is null. player id:%d", m_player->GetId());
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
            K_slog_trace(K_SLOG_ERROR, "[%s][%d] Invalid packet", __FUNCTION__, __LINE__);
            return false;
        }

        Dispatch(result.packet);
    }
    return true;
}

void ChannelSession::Dispatch(const ParsedPacket &pkt)
{
    if (pkt.type == PKT_CHANNEL_AUTH && m_server)
    {
        auto task = std::make_unique<ChannelAuthTask>(
            m_server,
            m_fd,
            pkt.payload
        );

        m_server->GetAuthThreadPool()->Submit(std::move(task));
        return;
    }

    if (m_server == nullptr)
        return;

    auto task = std::make_unique<PacketProcessTask>(
        m_server,
        this,
        m_fd,
        m_sessionId,
        m_generation,
        pkt.type,
        pkt.payload
    );

    m_server->GetThreadPool()->SubmitByKey(m_sessionId, std::move(task));
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

    for(payloadIter = payload.begin(); payloadIter < payload.end(); ++payloadIter)
    {
         K_slog_trace(K_SLOG_TRACE, "[%s][%d] body[%s]", __FUNCTION__, __LINE__, payloadIter->c_str());
    }
    payload.insert(payload.begin(), "ok");
    std::string body = PacketParser::MakeBody(payload);
    std::string packet = PacketParser::MakePacket(type, body);

   return EnqueueSend(std::move(packet));
}

int ChannelSession::SendNok(int type, const std::string &errMsg)
{
    std::vector<std::string> msg;
    msg.push_back("nok");
    msg.push_back(errMsg);
    std::string body = PacketParser::MakeBody(msg);
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

    }

    if (needEnableWrite && m_server != nullptr)
    {
        K_slog_trace(K_SLOG_TRACE, "[EnqueueSend] EnableWriteEvent fd:%d", m_fd);
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
