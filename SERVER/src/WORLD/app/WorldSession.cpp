#include "WorldSession.h"
#include <sys/socket.h>
#include <unistd.h>
#include "PacketParser.h"
#include "K_slog.h"


WorldSession::WorldSession(const int fd) : m_fd(fd), m_authenticated(false)
{
    //redis/db연결
    //redis/db연결
}
WorldSession::~WorldSession() {}

int WorldSession::OnPacket(const std::string& )
{
    return 0;
}

int WorldSession::Send(int type, const std::vector<std::string>& payload)
{
    std::string body = PacketParser::MakeBody(payload);
    std::string packet = PacketParser::MakePacket(type, body);
    send(m_fd, packet.c_str(), packet.size(), 0);
    return 0;
}

int WorldSession::SendOk(int type, std::vector<std::string> payload)
{
    payload.insert(payload.begin(), "ok");
    std::string body = PacketParser::MakeBody(payload);
    std::string packet = PacketParser::MakePacket(type, body);
    send(m_fd, packet.c_str(), packet.size(), 0);
    return 0;
}

int WorldSession::SendNok(int type, const std::string &errMsg)
{
    std::vector<std::string> msg;
    msg.push_back("nok");
    msg.push_back(errMsg);
    std::string body = PacketParser::MakeBody(msg);
    std::string packet = PacketParser::MakePacket(type, body);
    send(m_fd, packet.c_str(), packet.size(), 0);
    return 0;
}

int WorldSession::Close()
{
    if (m_fd > 0)
    {
        close(m_fd);
        m_fd = 0;
    }
    return 0;
}

bool WorldSession::IsAuthenticated() const
{
    return m_authenticated;
}

void WorldSession::SetAuthenticated(bool authenticated)
{
    m_authenticated = authenticated;
}