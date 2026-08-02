#include "common.h"
#include "WorldServer.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <string.h>
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
            K_LOG_ERROR( "select() error");
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

    if (sessionIt == m_sessions.end() ||sessionIt->second == nullptr)
    {
        K_LOG_ERROR("session not found. fd=%d",fd);

        return -1;
    }

    WorldSession* session = sessionIt->second;

    char temp[PacketLimits::kReceiveChunkSize];
    std::size_t totalReceivedLength = 0;

    while (true)
    {
        const ssize_t receivedLength = recv(fd,temp,sizeof(temp),0);

        if (receivedLength > 0)
        {
            session->m_recvBuffer.insert(session->m_recvBuffer.end(), temp,temp + receivedLength);
            totalReceivedLength += static_cast<std::size_t>(receivedLength);
            continue;
        }

        if (receivedLength == 0)
        {
            K_LOG_TRACE("client disconnected. fd=%d",fd);

            OnDisconnect(fd);
            return 1;
        }

        const int socketError = errno;

        if (socketError == EINTR)
        {
            continue;
        }

        if (socketError == EAGAIN || socketError == EWOULDBLOCK)
        {
            break;
        }

        K_LOG_ERROR("recv failed. fd=%d errno=%d",fd,socketError);

        OnDisconnect(fd);
        return 1;
    }

    K_LOG_DEBUG("recv from fd=%d, len=%zu",fd, totalReceivedLength);

    while (true)
    {
        ParseResult parseResult = PacketParser::TryParse(session->m_recvBuffer);

        if (parseResult.status == ParseStatus::NeedMoreData)
        {
            // 부분 패킷이므로 다음 수신까지 보관
            break;
        }

        if (parseResult.status == ParseStatus::InvalidPacket)
        {
            K_LOG_ERROR("invalid packet. fd=%d",fd);

            OnDisconnect(fd);
            return 1;
        }

        ParsedPacket packet = std::move(parseResult.packet);

        auto handler = m_factory.Create(packet.type);

        if (!handler)
        {
            K_LOG_ERROR("unknown packet type=%u fd=%d",packet.type,fd);

            continue;
        }

        PacketContext ctx{};
        ctx.world_session = session;
        ctx.char_service = &m_char_service;
        ctx.channel_manager = &m_channel_manager;
        ctx.redis_pool = &m_redisPool;
        ctx.fd = fd;
        ctx.payload = packet.payload.data();
        ctx.payload_len = static_cast<int>(packet.payload.size());
        handler->Execute(&ctx);
    }

    K_LOG_DEBUG("ProcessClient fd=%d done",fd);

    return 0;
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

