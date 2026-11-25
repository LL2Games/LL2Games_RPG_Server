#include "common.h"
#include "WorldServer.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <cerrno>
#include <unistd.h>
#include <cstdio>
#include <openssl/err.h>
#include <fcntl.h>
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

int WorldServer::Init(const int port,const RedisConfig& redisConfig, const char* certPath, const char* keyPath)
{
    m_tlsContext.reset(SSL_CTX_new(TLS_server_method()));

    if (!m_tlsContext ||
        SSL_CTX_set_min_proto_version(m_tlsContext.get(), TLS1_2_VERSION) != 1 ||
        SSL_CTX_use_certificate_chain_file(m_tlsContext.get(), certPath) != 1 ||
        SSL_CTX_use_PrivateKey_file(m_tlsContext.get(), keyPath, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(m_tlsContext.get()) != 1)
    {
        K_LOG_ERROR("[WORLD] TLS initialization failed");
        ERR_print_errors_fp(stderr);
        m_tlsContext.reset();
        return -1;
    }

    K_LOG_TRACE("[WORLD] TLS context initialized");
    
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
    m_running.store(true, std::memory_order_release);
    return 0;
}

int WorldServer::Run()
{
    fd_set reads;
    fd_set writes;

    while (m_running.load(std::memory_order_acquire))
    {
        FD_ZERO(&reads);
        FD_ZERO(&writes);
        FD_SET(m_listen_fd, &reads);

        int fd_max = m_listen_fd;
        bool hasBufferedTlsData = false;
        for (const auto& [fd, session] : m_sessions)
        {
            if (session->HasBufferedTlsData()) hasBufferedTlsData = true;
            using State = WorldSession::TlsHandshakeState;
            const State state = session->GetTlsHandshakeState();

            const bool watchWrite =
                state == State::WantWrite ||
                (state == State::Ready &&
                 (session->m_tlsReadWantsWrite ||
                  (session->HasPendingTlsWrites() && !session->m_tlsWriteWantsRead)));
                
            if (watchWrite)
                FD_SET(fd, &writes);
            else
                FD_SET(fd, &reads);

            if (fd > fd_max)
                fd_max = fd;
        }

        //RequestStop()을 최대 1초 안에 확인
        timeval timeout{};
        timeout.tv_sec = hasBufferedTlsData ? 0 : 1; 
        timeout.tv_usec = 0;

        const int ret = select(fd_max + 1, &reads, &writes, nullptr, &timeout);
        
        //select에서 깨어난 직후 종료 요청부터 확인
        if(!m_running.load(std::memory_order_acquire))
            break;
        
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;

            K_LOG_ERROR("[WORLD] select failed errno:%d", errno);
            break;
        }

        // 타임아웃
        if (ret == 0 && !hasBufferedTlsData)
            continue;

        if (FD_ISSET(m_listen_fd, &reads))
            OnAccept();

        std::vector<int> read2Fd;
        read2Fd.reserve(m_sessions.size());

        for (const auto& [fd, session] : m_sessions)
        {
            if (FD_ISSET(fd, &reads) || FD_ISSET(fd, &writes) || session->HasBufferedTlsData())
                read2Fd.push_back(fd);
        }

        for(int fd : read2Fd)
        {
            if(!m_running.load(std::memory_order_acquire))
                break;

            const auto it = m_sessions.find(fd);
            if (it == m_sessions.end())
                continue;

            WorldSession* session = it->second;
            using State = WorldSession::TlsHandshakeState;

            if (session->GetTlsHandshakeState() != State::Ready)
            {
                const State state = session->AdvanceTlsHandshake();
            
                if (state == State::Ready)
                    K_LOG_TRACE("[WORLD] TLS handshake complete fd:%d", fd);
                else if (state == State::Failed)
                    OnDisconnect(fd);
            
                continue;
            }

            if (session->m_tlsReadWantsWrite || !session->HasPendingTlsWrites())
            {
                OnReceive(fd);  // 내부에서 ReadTls()로 읽고 패킷 파싱
                continue;
            }

            using WriteResult = WorldSession::TlsWriteResult;
            const WriteResult result = session->FlushTlsWrites();

            if (result == WriteResult::Failed)
            {
                OnDisconnect(fd);
                continue;
            }

            session->m_tlsWriteWantsRead = (result == WriteResult::WantRead);
        }

    }
    K_LOG_TRACE("[WORLD] Run loop stopped");
    return 0;
}

void WorldServer::RequestStop() noexcept
{
    m_running.store(false, std::memory_order_release);
}

void WorldServer::ShutdownGracefully() noexcept
{
    RequestStop();

    BroadcastServerShutdown();
    DisconnectAllClients();

    if(m_listen_fd >= 0)
    {
        close(m_listen_fd);
        m_listen_fd = -1;
    }

    K_LOG_TRACE("[WORLD] Graceful shutdown completed");
}

void WorldServer::DisconnectAllClients() noexcept
{
    while(!m_sessions.empty())
    {
        const int fd = m_sessions.begin()->first;
        OnDisconnect(fd);
    }
}

void WorldServer::BroadcastServerShutdown()
{
    for(const auto& [fd, session] : m_sessions)
    {
        if(session == nullptr || session->GetTlsHandshakeState() != WorldSession::TlsHandshakeState::Ready)
            continue;

        session->Send(PKT_SERVER_SHUTDOWN_NOTIFY, {});
    }
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
    const int flags = fcntl(client_fd, F_GETFL,0 );
    if(client_fd >= FD_SETSIZE || flags == -1 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        close(client_fd);
        return -1;
    }

    auto session = std::make_unique<WorldSession>(client_fd);
    if(!session->AttachTls(m_tlsContext.get()))
    {
        K_LOG_ERROR("[WORLD] Failed to attach TLS fd:%d", client_fd );
        close(client_fd);
        return -1;
    }

    m_sessions.emplace(client_fd, session.get());
    session.release();

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

  using Result = WorldSession::TlsReadResult;

    auto& chunk = session->m_tlsReadChunk;
    std::size_t receivedSize = 0;
    const Result result =
        session->ReadTls(chunk.data(), chunk.size(), receivedSize);

    session->m_tlsReadWantsWrite = (result == Result::WantWrite);

    if (result == Result::WantRead || result == Result::WantWrite)
        return 0;

    if (result == Result::Closed || result == Result::Failed)
    {
        OnDisconnect(fd);
        return result == Result::Closed ? 1 : -1;
    }

    session->m_recvBuffer.insert(session->m_recvBuffer.end(), chunk.data(), chunk.data() + receivedSize);

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

