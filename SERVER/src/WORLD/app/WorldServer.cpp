#include "common.h"
#include "WorldServer.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <cerrno>
#include <unistd.h>
#include "PacketParser.h"

WorldServer::WorldServer() {}
WorldServer::~WorldServer() {}

int WorldServer::Init(const std::string &configPath)
{
    if(m_channel_manager.Init() != EXIT_SUCCESS)
    {
        return -1;
    }
    //server conf read, parsing
    K_LOG_DEBUG( "configPath[%s]", configPath); //test

    return 0;
}

int WorldServer::Init(const int port,const RedisConfig& redisConfig)
{

    if (!m_redisPool.Init(redisConfig, redisConfig.poolCount))
    {
        K_LOG_ERROR( "RedisConnectionPool init failed");
        return -1;
    }
    
    if(m_channel_manager.Init() != EXIT_SUCCESS)
    {
        K_LOG_ERROR( "m_channel_manager.Init failed");
        return -1;
    }

    m_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_fd < 0)
    {
        K_LOG_ERROR( "[%s] socket", WORLD_DAEMON_NAME);
        return -1;
    }

    int opt = 1;
    setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(m_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        K_LOG_ERROR( "[%s] bind [port=%d]", WORLD_DAEMON_NAME, port);
        return -1;
    }
    if (listen(m_listen_fd, 10) < 0)
    {
        K_LOG_ERROR( "[%s] listen", WORLD_DAEMON_NAME);
        return -1;
    }

    K_LOG_TRACE( "[%s] Listening on %d", WORLD_DAEMON_NAME, port);

    return 0;
}

int WorldServer::Run()
{
    int idx = 0;
    fd_set reads;

    while (true)
    {
        K_LOG_TRACE( "Run[%d]", ++idx);
        FD_ZERO(&reads);
        FD_SET(m_listen_fd, &reads);

        int fd_max = m_listen_fd;
        for (auto session : m_sessions)
        {
            const int sFd = session.first;
            FD_SET(sFd, &reads);
            if (sFd > fd_max) fd_max = sFd;
        }

        int ret = select(fd_max + 1, &reads, nullptr, nullptr, nullptr);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;

            K_LOG_ERROR("[WORLD] select failed errno:%d", errno);
            break;
        }

        if (FD_ISSET(m_listen_fd, &reads))
            OnAccept();
        std::vector<int> read2Fd;
        for (auto session : m_sessions)
        {
            if (FD_ISSET(session.first, &reads))
                read2Fd.push_back(session.first);
        }

        for(int fd : read2Fd)
        {
            OnReceive(fd);
        }

    }
    return 0;
}

int WorldServer::OnAccept()
{
    struct sockaddr_in clnt_addr{};
    socklen_t addr_len = sizeof(clnt_addr);

    int client_fd = accept(m_listen_fd, (struct sockaddr *)&clnt_addr, &addr_len);
    if (client_fd < 0)
    {
        K_LOG_ERROR( "accept");
        return -1;
    }

    K_LOG_TRACE( "client_accept[fd=%d]", client_fd);
    WorldSession *session = new WorldSession(client_fd);
    m_sessions[client_fd] = session;

    return 0;
}

int WorldServer::OnReceive(int fd)
{
     auto sessionIt = m_sessions.find(fd);

    if (sessionIt == m_sessions.end() || sessionIt->second == nullptr)
    {
        K_LOG_ERROR("[WORLD] session not found fd:%d", fd);
        return -1;
    }

    WorldSession* session = sessionIt->second;

    char receiveBuffer[PacketLimits::kReceiveChunkSize];
    ssize_t receivedSize = 0;

    // 시그널로 recv()가 중단된 경우에만 재시도한다.
    do
    {
        receivedSize = recv(fd, receiveBuffer, sizeof(receiveBuffer),0);
    }
    while (receivedSize < 0 && errno == EINTR);

    if (receivedSize == 0)
    {
        OnDisconnect(fd);
        return 1;
    }

    if (receivedSize < 0)
    {
        const int recvError = errno;

        K_LOG_ERROR("[WORLD] recv failed fd:%d errno:%d", fd, recvError);
        OnDisconnect(fd);
        return -1;
    }

    session->m_recvBuffer.insert(session->m_recvBuffer.end(), receiveBuffer, receiveBuffer + receivedSize);

    K_LOG_DEBUG("[WORLD] recv fd:%d size:%zd buffered:%zu", fd, receivedSize,session->m_recvBuffer.size());

    while (true)
    {
        ParseResult result = PacketParser::TryParse(session->m_recvBuffer);

        if (result.status == ParseStatus::NeedMoreData)
        {
            return 0;
        }

        if (result.status == ParseStatus::InvalidPacket)
        {
            K_LOG_ERROR("[WORLD] invalid packet fd:%d buffered:%zu",fd, session->m_recvBuffer.size() );
            OnDisconnect(fd);
            return -1;
        }

        ParsedPacket& packet = result.packet;
        K_LOG_DEBUG("[WORLD] packet parsed fd:%d type:%u payloadSize:%zu",fd,static_cast<unsigned int>(packet.type),packet.payload.size());

        if (!session->IsAuthenticated() &&
            packet.type != PKT_INIT_WORLD)
        {
            K_LOG_ERROR("[WORLD] unauthenticated packet rejected fd:%d type:%u",fd,static_cast<unsigned int>(packet.type));
            session->SendNok(packet.type,"World authentication required");

            // 거부된 패킷은 이미 버퍼에서 제거됐다.
            // 뒤에 이어진 패킷을 처리한다.
            continue;
        }

        auto handler = m_factory.Create(packet.type);

        if (!handler)
        {
            K_LOG_ERROR("[WORLD] handler not found fd:%d type:%u",fd, static_cast<unsigned int>(packet.type));
    
            continue;
        }

        PacketContext context{};
        context.world_session = session;
        context.char_service = &m_char_service;
        context.channel_manager = &m_channel_manager;
        context.redis_pool = &m_redisPool;
        context.fd = fd;
        context.type = packet.type;
        context.payload = packet.payload.empty() ? nullptr : packet.payload.data();
        context.payload_len = static_cast<int>(packet.payload.size());

        handler->Execute(&context);
        K_LOG_DEBUG("[WORLD] OnReceive fd:%d packet done",fd);
    }
}

int WorldServer::OnDisconnect(int fd)
{
    auto it = m_sessions.find(fd);
    if(it == m_sessions.end())
    {
        return 0;
    }

    K_LOG_TRACE( "Client %d disconnected", fd);
    close(fd);
    delete it->second;
    m_sessions.erase(it);

    return 0;
}

