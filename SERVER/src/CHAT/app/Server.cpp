#include "Server.h"
#include "Client.h"
#include "common.h"
#include "Packet.h"
#include "IPacketHandler.h"
#include "PacketParser.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

#define MSG_KEY 1234
#define MSG_COMMAND_SEND 1
#define MSG_COMMAND_RECV 2

Server::Server() : m_dispatcher(MSG_KEY, MSG_COMMAND_SEND, MSG_COMMAND_RECV)
{
}

bool Server::Init(const int port, const RedisConfig& redisConfig)
{
    if (!m_redisPool.Init(redisConfig, redisConfig.poolCount))
    {
        K_LOG_ERROR( "RedisConnectionPool init failed");
        return false;
    }
    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0)
    {
        K_LOG_ERROR( "[%s] socket", CHAT_DAEMON_NAME);
        return false;
    }

    int opt = 1;
    setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(m_listenFd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        K_LOG_ERROR( "[%s] bind [port=%d]", CHAT_DAEMON_NAME, port);
        return false;
    }
    if (listen(m_listenFd, 10) < 0)
    {
        K_LOG_ERROR( "[%s] listen", CHAT_DAEMON_NAME);
        return false;
    }

    K_LOG_TRACE( "[%s] Listening on %d", CHAT_DAEMON_NAME, port);

    return true;
}

void Server::Run()
{
    fd_set reads;

    while (true)
    {
        FD_ZERO(&reads);
        FD_SET(m_listenFd, &reads);

        int fd_max = m_listenFd;

        for (auto c : m_clients)
        {
            FD_SET(c->GetFD(), &reads);
            if (c->GetFD() > fd_max)
                fd_max = c->GetFD();
        }

        const int ret = select(fd_max + 1, &reads, nullptr, nullptr, nullptr);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;

            K_LOG_ERROR("[%s] select failed errno:%d",CHAT_DAEMON_NAME,errno);
            break;
        }

        // ProcessClient에서 m_clients가 변경될 수 있으므로
        // 읽기 가능한 클라이언트를 먼저 복사한다.
        std::vector<Client*> readableClients;
        readableClients.reserve(m_clients.size());

        for (Client* client : m_clients)
        {
            if (FD_ISSET(client->GetFD(), &reads))
                readableClients.push_back(client);
        }

        if (FD_ISSET(m_listenFd, &reads))
            AcceptNewClient();

        for (auto cli : m_clients)
        {
            if (FD_ISSET(cli->GetFD(), &reads))
                ProcessClient(cli);
        }
    }
}

void Server::AcceptNewClient()
{
    struct sockaddr_in clnt_addr{};
    socklen_t addr_len = sizeof(clnt_addr);

    int client_fd = accept(m_listenFd, (struct sockaddr *)&clnt_addr, &addr_len);
    if (client_fd < 0)
        return;

    Client *cli = new Client(client_fd);

    m_clients.push_back(cli);
    K_LOG_TRACE( "[%s] Client[fd=%d][id=%s][nick=%s] connected\n", "LOGIN", client_fd, cli->GetId().c_str(), cli->GetNick().c_str());
}

void Server::ProcessClient(Client *cli)
{
     if (cli == nullptr)
        return;

    const int fd = cli->GetFD();
    char receiveBuffer[PacketLimits::kReceiveChunkSize];
    ssize_t receivedSize = 0;

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
        const int recvError = errno;
        K_LOG_ERROR("[%s] recv failed fd:%d errno:%d", CHAT_DAEMON_NAME, fd, recvError);
        DisconnectClient(cli);
        return;
    }

    cli->m_recvBuffer.insert(cli->m_recvBuffer.end(), receiveBuffer, receiveBuffer + receivedSize);
    K_LOG_DEBUG("[CHAT] recv fd:%d size:%zd buffered:%zu", fd, receivedSize, cli->m_recvBuffer.size());

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
            K_LOG_ERROR("[CHAT] Invalid packet fd:%d buffered:%zu", fd, cli->m_recvBuffer.size());
            DisconnectClient(cli);
            return;
        }

        ParsedPacket& packet = result.packet;
        K_LOG_DEBUG("[CHAT] Packet parsed fd:%d type:%u payloadSize:%zu",fd, static_cast<unsigned int>(packet.type), packet.payload.size());
        auto handler = m_factory.Create(packet.type);

        if (!handler)
        {
            K_LOG_ERROR("[CHAT] Handler not found fd:%d type:%u", fd, static_cast<unsigned int>(packet.type));
            // 해당 패킷은 이미 버퍼에서 제거됐다.
            // 연결은 유지하고 다음 패킷을 처리한다.
            continue;
        }

        PacketContext context{};
        context.redis_pool = &m_redisPool;
        context.dispatcher = &m_dispatcher;
        context.client = cli;
        context.clients = &m_clients;
        context.type = packet.type;
        context.payload = packet.payload.empty() ? nullptr : packet.payload.data();
        context.payload_len = static_cast<int>(packet.payload.size());

        context.broadcast = [this](
                        const std::string &nick,
                        const std::string &msg,
                        const int execptFD)
        {
            BroadCast(nick, msg, execptFD);
        };

        // Execute가 동기적으로 실행되므로 packet.payload의 포인터가
        // handler 실행 중에는 유효하다.
        handler->Execute(&context);
         K_LOG_DEBUG("[%s] ProcessClient fd:%d packet done", CHAT_DAEMON_NAME,fd);
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

        K_LOG_TRACE("[CHAT] Client %d disconnected",fd);

        close(fd);
        delete client;
        m_clients.erase(it);
        return;
    }
}

void Server::BroadCast(const std::string &nick, const std::string &msg, const int exceptFd)
{
    std::vector<std::string> datas;
    datas.push_back(nick);
    datas.push_back(msg);
    std::string body = PacketParser::MakeBody(datas);
    std::string packet = PacketParser::MakePacket(PKT_CHAT, body);
    K_LOG_TRACE( "BroadCast msg[nick:%s][%s]", nick.c_str(), msg.c_str());

    K_LOG_DEBUG( "packet[size=%d]", packet.size());
    for (int i = 0; i < (int)packet.size(); i++)
    {
        K_LOG_DEBUG( "packet[%d]: %x", i, (uint8_t)packet[i]);
    }

    K_LOG_DEBUG( "BroadCast to clients count=%d", m_clients.size());
    for (auto cli : m_clients)
    {
        if (cli->GetFD() == exceptFd)
        {
            K_LOG_DEBUG( "skip exceptFd=%d", exceptFd);
            continue;
        }

        K_LOG_DEBUG( "gunoo22_TEST send to fd=%d", cli->GetFD());
        send(cli->GetFD(), packet.c_str(), packet.size(), 0);
        K_LOG_DEBUG( "gunoo22_TEST sent to fd=%d//", cli->GetFD());
    }
}
