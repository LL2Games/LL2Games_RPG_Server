#include "Server.h"
#include "PacketParser.h"
#include "K_slog.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <cerrno>
#include <fcntl.h>


bool Server::Init(int port, const RedisConfig& redisConfig, const char* certPath, const char* keyPath)
{
    m_tlsContext.reset(SSL_CTX_new(TLS_server_method()));
    if (!m_tlsContext ||
        SSL_CTX_set_min_proto_version(m_tlsContext.get(), TLS1_2_VERSION) != 1 ||
        SSL_CTX_use_certificate_chain_file(m_tlsContext.get(), certPath) != 1 ||
        SSL_CTX_use_PrivateKey_file(m_tlsContext.get(), keyPath, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(m_tlsContext.get()) != 1)
    {
        K_LOG_ERROR("[LOGIN] TLS initialization failed");
        return false;
    }

    if (!m_redisPool.Init(redisConfig, redisConfig.poolCount))
    {
        K_LOG_ERROR( "RedisConnectionPool init failed");
        return false;
    }
    m_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_fd < 0)
        return false;

    int opt = 1;
    setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(m_listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        return false;

    if (listen(m_listen_fd, 10) < 0)
        return false;

    K_LOG_TRACE( "[%s] Listening on port %d\n", "LOGIN", port);

    m_running.store(true, std::memory_order_release);
    return true;
}

void Server::Run()
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

        for (Client* client : m_clients)
        {
            if(client == nullptr)
                continue;

            const int fd = client->GetFD();

           using State = Client::TlsHandshakeState;

            const State state = client->GetTlsHandshakeState();
            const bool watchWrite =
                state == State::WantWrite ||(state == State::Ready &&(client->m_tlsReadWantsWrite ||
                     (client->HasPendingTlsWrites() && !client->m_tlsWriteWantsRead)));
                    
            if (watchWrite)
                FD_SET(fd, &writes);
            else
                FD_SET(fd, &reads);
                    
            if (client->HasBufferedTlsData())
                hasBufferedTlsData = true;

            if (fd > fd_max)
                fd_max = fd;
        }

        timeval timeout{};
        timeout.tv_sec = hasBufferedTlsData ? 0 : 1;
        timeout.tv_usec = 0;

        int ret = select(fd_max + 1, &reads, &writes, nullptr, &timeout);
        
        if(!m_running.load(std::memory_order_acquire))
            break;
        
        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            K_LOG_ERROR("[LOGIN] select failed errno:%d",errno);
            break;
        }

      
        // 신규 접속
        if (FD_ISSET(m_listen_fd, &reads))
            AcceptNewClient();

        std::vector<Client*> readableClients;
        readableClients.reserve(m_clients.size());

        for (Client* client : m_clients)
        {
            const int fd = client->GetFD();

            if (FD_ISSET(fd, &reads) || FD_ISSET(fd, &writes) || client->HasBufferedTlsData())
                readableClients.push_back(client);
        }

        for (Client* client : readableClients)
        {       
            if(!m_running.load(std::memory_order_acquire))
                break;
            using State = Client::TlsHandshakeState;

            if (client->GetTlsHandshakeState() == State::Ready)
            {
                if (client->m_tlsReadWantsWrite || !client->HasPendingTlsWrites())
                {
                    ProcessClient(client);
                }
                else
                {
                    using WriteResult = Client::TlsWriteResult;
                
                    const WriteResult result = client->FlushTlsWrites();
                    if (result == WriteResult::Failed)
                    {
                        DisconnectClient(client);
                        continue;
                    }
                
                    client->m_tlsWriteWantsRead = (result == WriteResult::WantRead);
                }
            
                continue;
            }

            client->AdvanceTlsHandshake();

            const State state = client->GetTlsHandshakeState();
            if (state == State::Ready)
                K_LOG_TRACE("[LOGIN] TLS handshake complete fd:%d", client->GetFD());
            else if (state == State::Failed)
                DisconnectClient(client);
        }
    }
    m_running.store(false, std::memory_order_release);

    K_LOG_TRACE("[LOGIN] Run loop stoppped");
}

void Server::RequestStop() noexcept
{
    m_running.store(false, std::memory_order_release);
}

void Server::ShutdownGracefully()
{
    RequestStop();

    BroadcastServerShutdown();
    DisconnectAllClients();

    if(m_listen_fd >= 0)
    {
        close(m_listen_fd);
        m_listen_fd = -1;
    }

    K_LOG_TRACE("[LOGIN] Graceful shutdown completed");
}

void Server::DisconnectAllClients() noexcept
{
    while(!m_clients.empty())
    {
        Client* client = m_clients.back();
        DisconnectClient(client);
    }
}

void Server::BroadcastServerShutdown() noexcept
{
    for(const auto& client : m_clients)
    {
        if(client == nullptr)
            continue;

        client->Send(PKT_SERVER_SHUTDOWN_NOTIFY, {});
    }
}

void Server::DisconnectClient(Client* client)
{
    if (client == nullptr)
        return;

    for (auto it = m_clients.begin(); it != m_clients.end(); ++it)
    {
        if (*it != client)
            continue;

        const int fd = client->GetFD();

        K_LOG_TRACE("[LOGIN] Client %d disconnected",fd);

        close(fd);
        delete client;
        m_clients.erase(it);
        return;
    }
}

void Server::AcceptNewClient()
{
    struct sockaddr_in clnt_addr{};
    socklen_t addr_len = sizeof(clnt_addr);

    int client_fd = accept(m_listen_fd, (struct sockaddr *)&clnt_addr, &addr_len);
    if (client_fd < 0)
        return;

    const int flags = fcntl(client_fd, F_GETFL, 0);
    if (client_fd >= FD_SETSIZE ||
        flags == -1 ||
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        K_LOG_ERROR("[LOGIN] Cannot prepare client socket fd:%d", client_fd);
        close(client_fd);
        return;
    }

    auto client = std::make_unique<Client>(client_fd);
    if (!client->AttachTls(m_tlsContext.get()))
    {
        K_LOG_ERROR("[LOGIN] Failed to attach TLS to fd:%d", client_fd);
        client.reset();
        close(client_fd);
        return;
    }

    m_clients.push_back(client.get());
    client.release();

    K_LOG_TRACE( "[%s] Client %d connected\n", "LOGIN", client_fd);
}

void Server::ProcessClient(Client *cli)
{
    if (cli == nullptr)
        return;

    const int fd = cli->GetFD();
    using Result = Client::TlsReadResult;

    auto& chunk = cli->m_tlsReadChunk;
    std::size_t receivedSize = 0;
    const Result result = cli->ReadTls(chunk.data(), chunk.size(), receivedSize);

    cli->m_tlsReadWantsWrite = (result == Result::WantWrite);

    if (result == Result::WantRead || result == Result::WantWrite)
        return;

    if (result == Result::Closed || result == Result::Failed)
    {
        DisconnectClient(cli);
        return;
    }

    cli->m_recvBuffer.insert(cli->m_recvBuffer.end(),chunk.data(),chunk.data() + receivedSize);
    K_LOG_DEBUG("[LOGIN] recv fd:%d size:%zu buffered:%zu", fd, receivedSize, cli->m_recvBuffer.size());

    while (true)
    {
        ParseResult result = PacketParser::TryParse(cli->m_recvBuffer);

        if (result.status == ParseStatus::NeedMoreData)
        {
            // 현재 버퍼를 유지하고 다음 recv를 기다린다.
            return;
        }

        if (result.status == ParseStatus::InvalidPacket)
        {
            // TryParse는 잘못된 패킷을 버퍼에서 제거하지 않는다.
            // 동일 패킷을 반복해서 처리하지 않도록 연결을 종료한다.
            K_LOG_ERROR("[LOGIN] Invalid packet fd:%d buffered:%zu", fd, cli->m_recvBuffer.size());
            DisconnectClient(cli);
            return;
        }

        ParsedPacket& packet = result.packet;
        K_LOG_DEBUG("[LOGIN] Packet parsed fd:%d type:%u payloadSize:%zu",fd, static_cast<unsigned int>(packet.type), packet.payload.size());
        auto handler = m_factory.Create(packet.type);

        if (!handler)
        {
            K_LOG_ERROR("[LOGIN] Handler not found fd:%d type:%u", fd, static_cast<unsigned int>(packet.type));
            // 해당 패킷은 이미 버퍼에서 제거됐다.
            // 연결은 유지하고 다음 패킷을 처리한다.
            continue;
        }

        PacketContext context{};
        context.redis_pool = &m_redisPool;
        context.client = cli;
        context.type = packet.type;

        context.payload = packet.payload.empty() ? nullptr : packet.payload.data();
        context.payload_len = static_cast<int>(packet.payload.size());

        // Execute가 동기적으로 실행되므로 packet.payload의 포인터가
        // handler 실행 중에는 유효하다.
        handler->Execute(&context);

        // 버퍼에 다음 완성 패킷이 남아 있을 수 있으므로
        // TryParse를 다시 호출한다.
    }

}