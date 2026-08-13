#include "Server.h"
#include "PacketParser.h"
#include "K_slog.h"

#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <cerrno>


bool Server::Init(int port, const RedisConfig& redisConfig)
{
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

    return true;
}

void Server::Run()
{
    fd_set reads;

    while (true)
    {
        FD_ZERO(&reads);
        FD_SET(m_listen_fd, &reads);
        int fd_max = m_listen_fd;

        for (auto c : m_clients)
        {
            FD_SET(c->GetFD(), &reads);
            if (c->GetFD() > fd_max)
                fd_max = c->GetFD();
        }

        int ret = select(fd_max + 1, &reads, nullptr, nullptr, nullptr);
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
            if (FD_ISSET(client->GetFD(), &reads))
            readableClients.push_back(client);
        }

        for (Client* client : readableClients)
        {       
            ProcessClient(client);
        }
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

    m_clients.push_back(new Client(client_fd));

    K_LOG_TRACE( "[%s] Client %d connected\n", "LOGIN", client_fd);
}

void Server::ProcessClient(Client *cli)
{
    if (cli == nullptr)
        return;

    const int fd = cli->GetFD();
    char receiveBuffer[PacketLimits::kReceiveChunkSize];
    ssize_t receivedSize = 0;

    // recv가 시그널로 중단된 경우에만 재시도
    do
    {
        receivedSize = recv(fd, receiveBuffer, sizeof(receiveBuffer),0);
    }
    while (receivedSize < 0 && errno == EINTR);

    if (receivedSize == 0)
    {
        DisconnectClient(cli);
        return;
    }

    if (receivedSize < 0)
    {
        K_LOG_ERROR("[LOGIN] recv failed fd:%d errno:%d",fd,errno);
        DisconnectClient(cli);
        return;
    }
    cli->m_recvBuffer.insert(cli->m_recvBuffer.end(), receiveBuffer, receiveBuffer + receivedSize);
    K_LOG_DEBUG("[LOGIN] recv fd:%d size:%zd buffered:%zu", fd, receivedSize, cli->m_recvBuffer.size());

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