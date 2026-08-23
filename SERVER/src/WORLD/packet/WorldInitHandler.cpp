#include "WorldInitHandler.h"
#include "AuthTicketService.h"
#include "K_slog.h"
#include "WorldSession.h"
#include "RedisConnectionPool.h"
#include "PacketParser.h"

void WorldInitHandler::Execute(PacketContext *ctx)
{
    if (ctx == nullptr)
    {
        K_LOG_ERROR("WorldInitHandler failed: context is null");
        return;
    }

    WorldSession* session = ctx->world_session;

    if (session == nullptr)
    {
        K_LOG_ERROR("WorldInitHandler failed: session is null");
        return;
    }

    if (session->IsAuthenticated())
    {
        K_LOG_ERROR("WorldInitHandler failed: session already authenticated");
        session->SendNok(PKT_INIT_WORLD,"World session is already authenticated");

        return;
    }

    if (ctx->redis_pool == nullptr)
    {
        K_LOG_ERROR("WorldInitHandler failed: Redis pool is null");
        session->SendNok(PKT_INIT_WORLD,"World authentication failed");
        return;
    }

    std::size_t offset = 0;
    std::string worldTicket;
    std::string parseError;

    if (!PacketParser::ParseLengthPrefixedString(
            ctx->payload,
            ctx->payload_len,
            offset,
            worldTicket,
            parseError))
    {
        K_LOG_ERROR("WorldInitHandler failed: invalid payload");

        session->SendNok(PKT_INIT_WORLD,"Invalid world authentication payload");

        return;
    }

    // 티켓 이외의 불필요한 필드가 포함된 패킷 거부
    if (offset != static_cast<std::size_t>(ctx->payload_len))
    {
        K_LOG_ERROR("WorldInitHandler failed: trailing payload");

        session->SendNok( PKT_INIT_WORLD,"Invalid world authentication payload");

        return;
    }

    RedisConnectionGuard redisGuard(ctx->redis_pool);

    if (!redisGuard)
    {
        K_LOG_ERROR("WorldInitHandler failed: Redis connection acquire failed");
        session->SendNok(PKT_INIT_WORLD,"World authentication failed");
        return;
    }

    const auto accountId = AuthTicketService::ConsumeWorldTicket(*redisGuard.Get(),worldTicket);
    if (!accountId.has_value())
    {
        K_LOG_ERROR("WorldInitHandler failed: invalid or expired ticket");
        session->SendNok(PKT_INIT_WORLD,"Invalid or expired world ticket");
        return;
    }

    // 클라이언트가 보낸 계정 ID가 아닌
    // Redis 티켓에 저장된 계정 ID 사용
    session->SetAccountid(*accountId);
    session->SetAuthenticated(true);
    session->SendOk(PKT_INIT_WORLD);
    K_LOG_TRACE("[FLOW][WORLD] world ticket consumed and session authenticated. fd[%d] accountId[%s]", session->GetFD(), accountId->c_str());
}