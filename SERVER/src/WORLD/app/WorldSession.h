#pragma once
#include <string>
#include <vector>
#include <memory>
#include <deque>
#include <array>
#include <openssl/ssl.h>
#include "Packet.h"
#include "K_slog.h"

struct SessionState
{

};

class WorldSession
{
public:
    WorldSession(const int fd);
    ~WorldSession();

    //Packet.h 형식으로 바꿀예정
    int GetFD () const { return m_fd; }
    std::string GetID () const { 
        K_LOG_DEBUG( "account_id[%s]", m_account_id.c_str());
        return m_account_id; }
    int SetAccountid(const std::string &id) {
        m_account_id = id;
        K_LOG_DEBUG( "account_id[%s]", m_account_id.c_str());
        return 0;
    }
    
    bool IsAuthenticated() const;
    void SetAuthenticated(bool authenticated);

public:
    int OnPacket(const std::string& packet);
    int Send(int type, const std::vector<std::string>& payload);
    int SendOk(int type, std::vector<std::string> payload = {});
    int SendNok(int type, const std::string& errMsg);
    int Close();

    std::vector<char> m_recvBuffer;
private:
    int m_fd;
    std::string m_account_id;
    std::string m_selected_char_id;
    SessionState m_state;

    bool m_authenticated;

public:
    enum class TlsHandshakeState { WantRead, WantWrite, Ready, Failed};
    enum class TlsReadResult { Data, WantRead, WantWrite, Closed, Failed };
    enum class TlsWriteResult { Drained, WantRead, WantWrite, Failed };
    
    bool AttachTls(SSL_CTX* context);
    bool HasPendingTlsWrites() const
    {
       return !m_tlsSendQueue.empty();
    }
 
    bool HasBufferedTlsData() const
    {
       return  GetTlsHandshakeState() == TlsHandshakeState::Ready &&
               !m_tlsReadWantsWrite &&
               !HasPendingTlsWrites() &&
               SSL_pending(m_tls.get()) > 0;
    }
    TlsHandshakeState AdvanceTlsHandshake();
    TlsReadResult ReadTls(char* buffer, std::size_t capacity, std::size_t& bytesRead);
    TlsWriteResult FlushTlsWrites();
    TlsHandshakeState GetTlsHandshakeState() const { return m_tlsHandshakeState;}
 
    std::array<char, PacketLimits::kReceiveChunkSize> m_tlsReadChunk{};
    bool m_tlsReadWantsWrite = false;
    bool m_tlsWriteWantsRead = false;
private:
    std::unique_ptr<SSL, decltype(&SSL_free)> m_tls{nullptr, SSL_free};
    TlsHandshakeState m_tlsHandshakeState = TlsHandshakeState::WantRead;
    std::deque<std::string> m_tlsSendQueue;
};