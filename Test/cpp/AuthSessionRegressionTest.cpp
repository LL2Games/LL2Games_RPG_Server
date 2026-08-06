#include "ChannelServerTestAccess.h"
#include "ChannelAuthResult.h"
#include "Player.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::unique_ptr<Player> MakeTestPlayer(
    int id,
    const std::string& name)
{
    auto player = std::make_unique<Player>();
    player->SetId(id);
    player->SetName(name);
    player->SetJob(100);
    player->SetLevel(1);
    player->SetMapId(100000000);
    player->SetPos(0.0f, 0.0f);
    return player;
}

void TestStaleAuthResultIsRejectedAfterFdReuse()
{
    ChannelServer server(1, 1, 100);
    constexpr int reusedFd = 42;

    ChannelSession oldSession(reusedFd, &server, 8, 1);
    ChannelServerTestAccess::AddSession(server, &oldSession);

    ChannelServerTestAccess::EraseSession(server, reusedFd);
    oldSession.MarkClosing();

    // 실제 재현 로그처럼 generation은 같고 sessionId만 다르다.
    ChannelSession newSession(reusedFd, &server, 9, 1);
    ChannelServerTestAccess::AddSession(server, &newSession);

    ChannelAuthResult staleResult;
    staleResult.fd = reusedFd;
    staleResult.sessionId = 8;
    staleResult.generation = 1;
    staleResult.characterId = 1001;
    staleResult.success = true;
    staleResult.player = MakeTestPlayer(1001, "stale_player");

    server.PushAuthResult(std::move(staleResult));
    ChannelServerTestAccess::ProcessAuthResults(server);

    Require(
        newSession.GetPlayer() == nullptr,
        "stale result was attached to reused fd session"
    );

    Require(
        server.GetPlayerManager()->GetPlayer(1001) == nullptr,
        "stale player was registered in PlayerManager"
    );

    ChannelServerTestAccess::EraseSession(server, reusedFd);

    std::cout
        << "[PASS] fd 재사용 후 오래된 인증 결과 거부\n";
}

void TestCurrentSessionIdentityAndClosingState()
{
    ChannelServer server(1, 1, 100);
    constexpr int fd = 42;

    ChannelSession session(fd, &server, 9, 1);
    ChannelServerTestAccess::AddSession(server, &session);

    Require(
        server.BeginValidSessionTask(fd, 8, 1) == nullptr,
        "old session identity was accepted"
    );

    ChannelSession* accepted =
        server.BeginValidSessionTask(fd, 9, 1);

    Require(
        accepted == &session,
        "current session identity was rejected"
    );

    server.EndSessionTask(accepted);
    session.MarkClosing();

    Require(
        server.BeginValidSessionTask(fd, 9, 1) == nullptr,
        "closing session was accepted"
    );

    ChannelServerTestAccess::EraseSession(server, fd);

    std::cout
        << "[PASS] 현재 세션 승인 및 종료 중 세션 거부\n";
}
}

int main()
{
    try
    {
        TestStaleAuthResultIsRejectedAfterFdReuse();
        TestCurrentSessionIdentityAndClosingState();
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return 1;
    }

    return 0;
}