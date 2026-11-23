#pragma once
#include <string>
#include <vector>
#include <memory>
#include <openssl/ssl.h>
#include <array>
#include <deque>

#include "Packet.h"
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
    std::array<char, PacketLimits::kReceiveChunkSize> m_tlsReadChunk{};
    bool m_tlsReadWantsWrite = false;

public:
    enum class TlsHandshakeState
    {
        WantRead,   // 소켓으로 데이터가 들어오면 다시 시도
        WantWrite,  // 소켓에 쓸 수 있게 되면 다시 시도
        Ready,      // 핸드셰이크 완료
        Failed     // 복구할 수 없는 오류. 연결 종료 대상
    };

    enum class TlsReadResult
    {
        Data,
        WantRead,
        WantWrite,
        Closed,
        Failed
    };

    enum class TlsWriteResult
    {
        Drained,    // 보낼 패킷을 모두 처리함
        WantRead,   // 읽기 이벤트가 오면 쓰기 재시도
        WantWrite,  // 쓰기 이벤트가 오면 쓰기 재시도
        Failed
    };

    bool AttachTls(SSL_CTX* context);
    bool HasPendingTlsWrites() const;
    bool m_tlsWriteWantsRead = false;

    bool HasBufferedTlsData() const
    {
        return GetTlsHandshakeState() == TlsHandshakeState::Ready
            && !m_tlsReadWantsWrite
            && !HasPendingTlsWrites()
            && SSL_pending(m_tls.get()) > 0;
    }

    void AdvanceTlsHandshake();

    TlsHandshakeState GetTlsHandshakeState() const{return m_tlsHandshakeState;}
    TlsReadResult ReadTls(char* buffer, std::size_t capacity, std::size_t& bytesRead);
    TlsWriteResult FlushTlsWrites();
private:
    int m_fd;
    std::string m_id;
    std::string m_nick;

private:
    std::unique_ptr<SSL, decltype(&SSL_free)> m_tls{nullptr, SSL_free};
    TlsHandshakeState m_tlsHandshakeState = TlsHandshakeState::WantRead;
    std::deque<std::string> m_tlsSendQueue;
};