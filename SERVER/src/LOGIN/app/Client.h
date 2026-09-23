#pragma once
#include <string>
#include <vector>

// Reactor

class Client{
public:
    Client(int fd);
    ~Client();

    
    // 버퍼
    
    int GetFD() const;
    std::string GetID() const;
    std::string GetNick() const;
    int Send(int type, const std::vector<std::string>& payload);
    int SendOk(const int type, std::vector<std::string> payload = {});
    int SendNok(const int type, const std::string &errMsg);
    
    std::vector<char> m_recvBuffer;
private:
    int m_fd;
    std::string m_id;
    std::string m_nick;
};