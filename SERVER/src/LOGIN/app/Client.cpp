#include "Client.h"
#include "PacketParser.h"
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/err.h>

Client::Client(int fd) : m_fd(fd) {}
Client::~Client() {}

int Client::GetFD() const { return m_fd; }
std::string Client::GetID() const { return m_id; }
std::string Client::GetNick() const { return m_nick; }

int Client::Send(int type, const std::vector<std::string>& payload)
{
    const std::string body = PacketParser::MakeBody(payload);
    m_tlsSendQueue.push_back(PacketParser::MakePacket(type, body));
    return EXIT_SUCCESS;
}

int Client::SendOk(const int type, std::vector<std::string> payload)
{
    payload.insert(payload.begin(), "ok");
    return Send(type, payload);
}

int Client::SendNok(const int type, const std::string &errMsg)
{
    return Send(type, {"nok", errMsg});
}

 bool Client::AttachTls(SSL_CTX* context)
 {
    m_tls.reset(SSL_new(context));
    return m_tls && SSL_set_fd(m_tls.get(), m_fd) == 1;
 }

 void Client::AdvanceTlsHandshake()
{
    ERR_clear_error();

    const int result = SSL_accept(m_tls.get());
    if (result == 1)
    {
        m_tlsHandshakeState = TlsHandshakeState::Ready;
        return;
    }

    switch (SSL_get_error(m_tls.get(), result))
    {
    case SSL_ERROR_WANT_READ:
        m_tlsHandshakeState = TlsHandshakeState::WantRead;
        break;
    case SSL_ERROR_WANT_WRITE:
        m_tlsHandshakeState = TlsHandshakeState::WantWrite;
        break;
    default:
        m_tlsHandshakeState = TlsHandshakeState::Failed;
        break;
    }
}

Client::TlsReadResult Client::ReadTls(char* buffer, std::size_t capacity, std::size_t& bytesRead)
{
    bytesRead = 0;
    ERR_clear_error();

    const int result = SSL_read_ex(m_tls.get(), buffer, capacity, &bytesRead);
    if (result == 1)
        return TlsReadResult::Data;

    switch (SSL_get_error(m_tls.get(), result))
    {
    case SSL_ERROR_WANT_READ:
        return TlsReadResult::WantRead;
    case SSL_ERROR_WANT_WRITE:
        return TlsReadResult::WantWrite;
    case SSL_ERROR_ZERO_RETURN:
        return TlsReadResult::Closed;
    default:
        return TlsReadResult::Failed;
    }
}

bool Client::HasPendingTlsWrites() const
{
    return !m_tlsSendQueue.empty();
}

Client::TlsWriteResult Client::FlushTlsWrites()
{
    while (!m_tlsSendQueue.empty())
    {
        const std::string& packet = m_tlsSendQueue.front();
        std::size_t bytesWritten = 0;

        ERR_clear_error();
        const int result = SSL_write_ex(m_tls.get(), packet.data(), packet.size(), &bytesWritten);

        if (result == 1)
        {
            // 기본 쓰기 모드에서는 성공 시 패킷 전체가 처리된다.
            m_tlsSendQueue.pop_front();
            continue;
        }

        switch (SSL_get_error(m_tls.get(), result))
        {
        case SSL_ERROR_WANT_READ:
            return TlsWriteResult::WantRead;
        case SSL_ERROR_WANT_WRITE:
            return TlsWriteResult::WantWrite;
        default:
            return TlsWriteResult::Failed;
        }
    }

    return TlsWriteResult::Drained;
}