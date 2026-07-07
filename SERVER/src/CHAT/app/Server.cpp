#include "Server.h"
#include "Client.h"
#include "common.h"
#include "Packet.h"
#include "IPacketHandler.h"
#include "PacketParser.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

#define BUFFER_SIZE 1024

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
        K_slog_trace(K_SLOG_ERROR, "RedisConnectionPool init failed");
        return false;
    }
    m_listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] socket", CHAT_DAEMON_NAME);
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
        K_slog_trace(K_SLOG_ERROR, "[%s] bind [port=%d]", CHAT_DAEMON_NAME, port);
        return false;
    }
    if (listen(m_listenFd, 10) < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] listen", CHAT_DAEMON_NAME);
        return false;
    }

    K_slog_trace(K_SLOG_TRACE, "[%s] Listening on %d", CHAT_DAEMON_NAME, port);

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

        int ret = select(fd_max + 1, &reads, nullptr, nullptr, nullptr);
        if (ret < 0)
        {
            K_slog_trace(K_SLOG_ERROR, "select() error");
            break;
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
    K_slog_trace(K_SLOG_TRACE, "[%s] Client[fd=%d][id=%s][nick=%s] connected\n", "LOGIN", client_fd, cli->GetId().c_str(), cli->GetNick().c_str());
}

void Server::ProcessClient(Client *cli)
{
    char temp[BUFFER_SIZE];
    int tempLen = 0;
    std::string buf;

    do
    {
        memset(temp, 0x00, sizeof(temp));
        tempLen = recv(cli->GetFD(), temp, sizeof(temp), 0);
        if (tempLen <= 0)
        {
            // disconnect
            K_slog_trace(K_SLOG_TRACE, "[%s] Client %d disconnected", CHAT_DAEMON_NAME, cli->GetFD());
            close(cli->GetFD());
            for (auto it = m_clients.begin(); it != m_clients.end(); it++)
            {
                if (*it == cli)
                {
                    delete cli;
                    m_clients.erase(it);
                    break;
                }
            }
            return;
        }
        buf.append(temp, tempLen);
    } while (tempLen == BUFFER_SIZE);

    cli->m_recvBuffer.insert(cli->m_recvBuffer.end(), buf.begin(), buf.end());

    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] recv from fd=%d, len=%d", __FUNCTION__, __LINE__, cli->GetFD(), buf.size());
    auto pkt = PacketParser::Parse(cli->m_recvBuffer);
    if (!pkt.has_value())
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] Packet Parse failed", __FUNCTION__, __LINE__);
        return;
    }
    

    auto handler = m_factory.Create(pkt->type);
    PacketContext ctx;
    ctx.redis_pool = &m_redisPool;
    ctx.dispatcher = &m_dispatcher;
    ctx.client = cli;
    ctx.clients = &m_clients;
    ctx.payload = (char *)pkt->payload.c_str();
    ctx.payload_len = pkt->payload.size(); 
    ctx.broadcast = [&](const std::string &nick,
                        const std::string &msg,
                        const int execptFD)
    {
        this->BroadCast(nick, msg, execptFD);
    };
    if (handler)
    {
        handler->Execute(&ctx);
    }
    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] ProcessClient fd=%d done", __FUNCTION__, __LINE__, cli->GetFD());
}

void Server::BroadCast(const std::string &nick, const std::string &msg, const int exceptFd)
{
    std::vector<std::string> datas;
    datas.push_back(nick);
    datas.push_back(msg);
    std::string body = PacketParser::MakeBody(datas);
    std::string packet = PacketParser::MakePacket(PKT_CHAT, body);
    K_slog_trace(K_SLOG_TRACE, "[%s][%d] BroadCast msg[nick:%s][%s]", __FUNCTION__, __LINE__, nick.c_str(), msg.c_str());

    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] packet[size=%d]", __FUNCTION__, __LINE__, packet.size());
    for (int i = 0; i < (int)packet.size(); i++)
    {
        K_slog_trace(K_SLOG_DEBUG, "[%s][%d] packet[%d]: %x", __FUNCTION__, __LINE__, i, (uint8_t)packet[i]);
    }

    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] BroadCast to clients count=%d", __FUNCTION__, __LINE__, m_clients.size());
    for (auto cli : m_clients)
    {
        if (cli->GetFD() == exceptFd)
        {
            K_slog_trace(K_SLOG_DEBUG, "[%s][%d] skip exceptFd=%d", __FUNCTION__, __LINE__, exceptFd);
            continue;
        }

        K_slog_trace(K_SLOG_DEBUG, "[%s][%d] gunoo22_TEST send to fd=%d", __FUNCTION__, __LINE__, cli->GetFD());
        send(cli->GetFD(), packet.c_str(), packet.size(), 0);
        K_slog_trace(K_SLOG_DEBUG, "[%s][%d] gunoo22_TEST sent to fd=%d//", __FUNCTION__, __LINE__, cli->GetFD());
    }
}
