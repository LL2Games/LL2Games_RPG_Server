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
    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] configPath[%s]", __FUNCTION__, __LINE__, configPath); //test

    return 0;
}

int WorldServer::Init(const int port,const RedisConfig& redisConfig)
{

    if (!m_redisPool.Init(redisConfig, redisConfig.poolCount))
    {
        K_slog_trace(K_SLOG_ERROR, "RedisConnectionPool init failed");
        return -1;
    }
    
    if(m_channel_manager.Init() != EXIT_SUCCESS)
    {
        K_slog_trace(K_SLOG_ERROR, "m_channel_manager.Init failed");
        return -1;
    }

    m_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_fd < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] socket", WORLD_DAEMON_NAME);
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
        K_slog_trace(K_SLOG_ERROR, "[%s] bind [port=%d]", WORLD_DAEMON_NAME, port);
        return -1;
    }
    if (listen(m_listen_fd, 10) < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] listen", WORLD_DAEMON_NAME);
        return -1;
    }

    K_slog_trace(K_SLOG_TRACE, "[%s] Listening on %d", WORLD_DAEMON_NAME, port);

    return 0;
}

int WorldServer::Run()
{
    int idx = 0;
    fd_set reads;

    while (true)
    {
        K_slog_trace(K_SLOG_TRACE, "[%s][%d] Run[%d]", __FUNCTION__, __LINE__, ++idx);
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
            K_slog_trace(K_SLOG_ERROR, "select() error");
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
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] accept", __FUNCTION__, __LINE__);
        return -1;
    }

    K_slog_trace(K_SLOG_TRACE, "[%s][%d] client_accept[fd=%d]", __FUNCTION__, __LINE__, client_fd);
    WorldSession *session = new WorldSession(client_fd);
    m_sessions[client_fd] = session;

    return 0;
}

int WorldServer::OnReceive(int fd)
{
    char temp[BUFFER_SIZE];
    int tempLen = 0;
    std::string buf;
    WorldSession *session = m_sessions[fd];

    if (session == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] session error", __FUNCTION__, __LINE__);
        return -1;
    }

    do
    {
        memset(temp, 0x00, sizeof(temp));
        tempLen = recv(fd, temp, sizeof(temp), 0);
        if (tempLen <= 0)
        {
            OnDisconnect(fd);    
            return 1;
        }
        buf.append(temp, tempLen);
    } while (tempLen == BUFFER_SIZE);

    session->m_recvBuffer.insert(session->m_recvBuffer.end(), buf.begin(), buf.end());

    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] recv from fd=%d, len=%d", __FUNCTION__, __LINE__, fd, buf.size());
    auto pkt = PacketParser::Parse(session->m_recvBuffer);
    if (!pkt.has_value())
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] Packet Parse failed", __FUNCTION__, __LINE__);
        return -1;
    }
    

    auto handler = m_factory.Create(pkt->type);
    PacketContext ctx;
    ctx.world_session = m_sessions[fd];
    ctx.char_service = &m_char_service;
    ctx.channel_manager = &m_channel_manager;
    ctx.redis_pool = &m_redisPool;
    ctx.fd = fd;
    ctx.payload = (char *)pkt->payload.c_str();
    ctx.payload_len = pkt->payload.size();
    if (handler)
    {
        handler->Execute(&ctx);
    }
    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] ProcessClient fd=%d done", __FUNCTION__, __LINE__, fd);

    return 0;
}

int WorldServer::OnDisconnect(int fd)
{
    auto it = m_sessions.find(fd);
    if(it == m_sessions.end())
    {
        return 0;
    }

    K_slog_trace(K_SLOG_TRACE, "[%s : %s : %d ] Client %d disconnected",
                 __FILE__, __FUNCTION__, __LINE__, fd);
    close(fd);
    delete it->second;
    m_sessions.erase(it);

    return 0;
}

