#include "WorldSession.h"
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/err.h>
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
    const std::string body = PacketParser::MakeBody(payload);
    m_tlsSendQueue.push_back(PacketParser::MakePacket(type, body));
    return 0;
}

int WorldSession::SendOk(int type, std::vector<std::string> payload)
{
    payload.insert(payload.begin(), "ok");
    return Send(type, payload);
}

int WorldSession::SendNok(int type, const std::string &errMsg)
{
    return Send(type, {"nok", errMsg});
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

bool WorldSession::AttachTls(SSL_CTX* context)
{
    m_tls.reset(SSL_new(context));

    if(!m_tls || SSL_set_fd(m_tls.get(), m_fd) != 1)
    {
        m_tls.reset();
        return false;
    }

    return true;
}

WorldSession::TlsHandshakeState WorldSession::AdvanceTlsHandshake()
{
    ERR_clear_error();

    const int result = SSL_accept(m_tls.get());

    if(result == 1)
    {
        m_tlsHandshakeState = TlsHandshakeState::Ready;
        return m_tlsHandshakeState;
    }

    const int sslError = SSL_get_error(m_tls.get(), result);
    switch(sslError)
    {
        case SSL_ERROR_WANT_READ:
            m_tlsHandshakeState = TlsHandshakeState::WantRead;
            break;
        case SSL_ERROR_WANT_WRITE:
            m_tlsHandshakeState = TlsHandshakeState::WantWrite;
            break;
        default:
            K_LOG_ERROR("[WORLD] TLS handshake failed fd:%d sslError:%d", m_fd, sslError);
            m_tlsHandshakeState = TlsHandshakeState::Failed;
            break;
    }

    return m_tlsHandshakeState;
}

WorldSession::TlsReadResult WorldSession::ReadTls(char* buffer, std::size_t capacity, std::size_t& bytesRead)
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

WorldSession::TlsWriteResult WorldSession::FlushTlsWrites()
{
    while (!m_tlsSendQueue.empty())
    {
        const std::string& packet = m_tlsSendQueue.front();
        std::size_t bytesWritten = 0;
        ERR_clear_error();

        const int result = SSL_write_ex(
            m_tls.get(), packet.data(), packet.size(), &bytesWritten);

        if (result == 1)
        {
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