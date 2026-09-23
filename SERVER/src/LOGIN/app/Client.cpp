#include "Client.h"
#include "PacketParser.h"
#include <sys/socket.h>
#include <unistd.h>

Client::Client(int fd) : m_fd(fd) {}
Client::~Client() {}

int Client::GetFD() const { return m_fd; }
std::string Client::GetID() const { return m_id; }
std::string Client::GetNick() const { return m_nick; }

int Client::Send(int type, const std::vector<std::string>& payload)
{
    std::string body = PacketParser::MakeBody(payload);
    std::string packet = PacketParser::MakePacket(type, body);

    const ssize_t sentSize = send(m_fd, packet.data(), packet.size(), MSG_NOSIGNAL);

    return sentSize == static_cast<ssize_t>(packet.size()) ? EXIT_SUCCESS : EXIT_FAILURE;
}

int Client::SendOk(const int type, std::vector<std::string> payload)
{
    payload.insert(payload.begin(), "ok");
    std::string body = PacketParser::MakeBody(payload);
    std::string packet = PacketParser::MakePacket(type, body);
    send(m_fd, packet.c_str(), packet.size(), 0);
    return 0;
}

int Client::SendNok(const int type, const std::string &errMsg)
{
    std::vector<std::string> msg;
    msg.push_back("nok");
    msg.push_back(errMsg);
    std::string body = PacketParser::MakeBody(msg);
    std::string packet = PacketParser::MakePacket(type, body);
    send(m_fd, packet.c_str(), packet.size(), 0);
    return 0;
}