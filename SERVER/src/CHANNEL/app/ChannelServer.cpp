#include "ChannelServer.h"
#include "common.h"
#include "thread"
#include "RedisClient.h"
#include "RedisUtility.h"
#include "ChannelStateUpdateTask.h"
#include "RedisCommonEnum.h"
#include "PlayerPacketSender.h"
#include "QuickSlotPacketSender.h"
#include "InventoryPacketSender.h"

//threadCount == 0 인경우 하드웨어 CPU 코어 수를 계산하여 스레드풀 생성
ChannelServer::ChannelServer(const int channelId, const int threadCount, const int maxUserCount) : m_channel_id(channelId), m_listen_fd(0), m_epfd(0), m_running(false), m_map_manager(this), m_map_service(m_player_mamager, m_map_manager), m_pool(threadCount == 0 ? std::thread::hardware_concurrency() : threadCount), m_authPool(threadCount == 0 ? std::thread::hardware_concurrency() : threadCount),m_level_manager(nullptr),m_current_user_count(0), m_max_user_count(maxUserCount)
{
    m_item_manager = ItemManager::GetInstance();
    m_monster_manager = MonsterManager::GetInstance();
    m_skill_manager = SkillManager::GetInstance();
    m_drop_manager = DropManager::GetInstance();
    m_level_manager = LevelManager::GetInstance();
}

ChannelServer::~ChannelServer()
{

}

int ChannelServer::SetNonblocking(int fd)
{
    // fd의 현재 설정된 값을 가지고 온다.
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    // fd의 현재 설정된 값을 유지하면서 non_block 옵션을 추가로 설정해준다.
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -1;
    return 0;
}

// FlushSend()로 송신 큐를 전부 비운 뒤, 그 fd의 EPOLLOUT 감시를 꺼주는 함수
// EPOLLOUT은 대부분의 경우 계속 발생할 수 있다.
// TCP 소켓은 보통 쓸 수 있는 상태인 시간이 많기 떄문에 보낼 데이터가 없는데도
// EPOLLOUT을 켜두면 epoll_wait()가 계속 깨어날 수 있기 때문이다.
// epoll_wait()가 꺠어난다는 의미는 epoll에서 이벤트가 발생했다는 의미이다
void ChannelServer::DisableWriteEvent(int fd)
{
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLRDHUP;

    if (epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &ev) < 0)
    {
        K_slog_trace(
            K_SLOG_ERROR,
            "[%s : %s : %d] EPOLL_CTL_MOD disable write failed fd:%d errno:%d",
            __FILE__,
            __FUNCTION__,
            __LINE__,
            fd,
            errno
        );
    }
}

void ChannelServer::OnSend(int fd)
{
    ChannelSession* session = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);

        auto it = m_sessions.find(fd);
        if (it == m_sessions.end())
            return;

        session = it->second;
    }

    if (!session->FlushSend())
    {
        OnDisconnect(fd);
        return;
    }

    if (!session->HasPendingSend())
    {
        DisableWriteEvent(fd);
    }
}

bool ChannelServer::Init(const int port, const RedisConfig& redisConfig)
{
    // 서버 구동 시 JSON 파일을 읽어온다.
    if(!m_map_manager.Init()) return false;
    if(!m_item_manager->Init()) return false;
    if(!m_monster_manager->Init()) return false;
    if(!m_skill_manager->Init()) return false;
    if(!m_drop_manager->Init()) return false;
    if(!m_level_manager->Init()) return false;


    if(!InitListenSocket(port))
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] InitListenSocket %d\n", "ChannelServer", port); 
        return false;
    }

    if(!InitEpoll())
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] InitEpoll Error %d\n", "ChannelServer", port); 
        return false;
    }

   K_slog_trace(K_SLOG_TRACE, "MAX_USER_COUNT: %d\n", m_max_user_count);
   K_slog_trace(K_SLOG_TRACE, "Thread Pool Start ==PoolSize: %zu\n", m_pool.GetPoolSize());
   K_slog_trace(K_SLOG_TRACE, "Auth Thread Pool Start ==PoolSize: %zu\n", m_authPool.GetPoolSize());
   //스레드풀 시작
   m_pool.Start();
   K_slog_trace(K_SLOG_TRACE, "ChatD MessageQueue Start\n");
   //chatD 메시지큐 리시버 스레드 시작
    m_authPool.Start();
    //m_cmd_receiver.Start(); 지금 미사용 나중에 다시 풀어야함
   
   //맵매니저 스레드 시작
   m_map_manager.Start();
    if (!m_redisPool.Init(redisConfig, redisConfig.poolCount))
    {
        K_slog_trace(K_SLOG_ERROR, "[ChannelServer] RedisConnectionPool Init failed");
        return false;
    }
   //채널 상태 업데이트 스레드 시작
    std::thread stateUpdateThread(&ChannelServer::UpdateChannelState, this, 3, 10); // 3초마다 업데이트, TTL은 10초
    stateUpdateThread.detach(); // 스레드를 분리하여 백그라운드에서 실행

   return true;
}

bool ChannelServer::InitListenSocket(int port)
{
    m_listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(m_listen_fd < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] m_listen_fd Set Error %d\n", "ChannelServer", port); 
        return false;
    }

    int opt = 1;
    if(setsockopt(m_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] m_listen_fd Setsockopt Error %d\n", "ChannelServer", port);
        close(m_listen_fd);
        m_listen_fd =-1;
        return false;
    }

    int nodelay = 1;
    if(setsockopt(m_listen_fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay)) < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] m_listen_fd Setsockopt Error %d\n", "ChannelServer", port);
        close(m_listen_fd);
        m_listen_fd =-1;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if(bind(m_listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))< 0)
    {   
        K_slog_trace( K_SLOG_ERROR,
        "[%s][%s][%d] bind Error port:%d errno:%d msg:%s\n",
    "ChannelServer", __FILE__, __LINE__, port, errno, strerror(errno));
        close(m_listen_fd);
        m_listen_fd = -1;
        return false;
    }

    int backlog = static_cast<int>(m_max_user_count);
    if (backlog < 1024)
        backlog = 1024;

    if(listen(m_listen_fd, backlog) < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%s][%d] m_listen_fd listen Error %d\n", "ChannelServer", __FILE__, __LINE__, port);
        close(m_listen_fd);
        m_listen_fd = -1;
        return false;
    }

    if(SetNonblocking(m_listen_fd) < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] SerNonblocking Error %d\n", "ChannelServer", port);
        close(m_listen_fd);
        m_listen_fd = -1;
        return false;
    }

    K_slog_trace(K_SLOG_TRACE, "[%s] Listening on Port %d\n", "ChannelServer", port);
    return true;


}

bool ChannelServer::InitEpoll()
{
    // 서버 시작 시점에는 아직 클라이언트 소켓이 존재하지 않기 때문에,
    // 먼저 listen 소켓을 epoll의 관심 목록에 등록한다.
    // 이렇게 하면 클라이언트의 접속 요청이 들어왔을 때 epoll_wait()를 통해
    // accept 가능한 상태가 되었음을 감지할 수 있다.
    // epoll 감시기 생성 -> 성공하면 0 반환 실패 시 -1 반환
    m_epfd = epoll_create1(EPOLL_CLOEXEC);
    if(m_epfd < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] m_epfd epoll_create Error %d\n", "ChannelServer", m_epfd);
        return false;
    }

    epoll_event ev{};
    ev.events = EPOLLIN; //읽기 가능한 이벤트를 감시하겠다 라고 설정 / 리슨 소켓에서는 read 가능 상태임을 뜻한다.
    ev.data.fd = m_listen_fd; // 어떤 fd에서 이벤트가 발생했는지 식별하기 위해서 설정

    // m_epfd의 관심 목록에 m_listem_fd를 추가하는 함수 / 즉 이 epoll 인스턴스가 이제부터 리슨 소켓을 감시해라 그리고 읽기 이벤트가 오면 알려줘라 라는 뜻이다.
    if(epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_listen_fd, &ev) < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] m_epfd epoll 속성 설정 실패 %d\n", "ChannelServer", m_epfd);
        close(m_epfd);
        m_epfd = -1;
        return false;
    }
    // 이벤트를 받아 담을 공간을 미리 설정
    m_events.resize(64);
    return true;
}

void ChannelServer::Run()
{
    if(m_listen_fd < 0 || m_epfd < 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s] m_listen_fd or m_epfd is Not initialized %d\n", "ChannelServer", m_epfd);
        return;
    }

    m_running = true;
    GameLoop();

}

void ChannelServer::GameLoop()
{
    while(true)
    {
        // m_epfd에 등록된 관심 목록에서 이벤트가 발생한 것들을 기다렸다가 m_events 배열에 채워 넣고 발생한 이벤트 개수를 n에 저장/ -1은 무한 대기의 의미 이벤트가 발생할 때 까지 계속 블로킹 
        int n = epoll_wait(m_epfd, m_events.data(), static_cast<int>(m_events.size()), 10);

        if( n < 0)
        {
            // EINTR : 보통 시그널 때문에 잠깐 인터럽트된 경우
            if(errno == EINTR) continue;
            break;
        }

        for(int i = 0; i < n; ++i)
        {
            int fd = m_events[i].data.fd;
            uint32_t e = m_events[i].events;

            // 새로운 클라이언트의 접속이라면 OnAccept 함수 실행
            if(fd == m_listen_fd)
            {
                OnAccept();
                continue;
            }
            
            // EPOLLERR : 소켓 에러 / EPOLLHUP : hang up, 연결 끊김 or 상대와의 연결이 정상 상태가 아님 / EPOLLRDHUP : 상대방이 read 방향 종료, 보통 원격 종료 감지용
            if(e & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                OnDisconnect(fd);
                continue;
            }

            if (e & EPOLLIN)
            {
                OnReceive(fd);
            }

            {
                std::lock_guard<std::mutex> lock(m_sessionMutex);

                if(m_sessions.find(fd) == m_sessions.end())
                    continue;
            }

            if(e & EPOLLOUT)
            {
                OnSend(fd);
            }
        }
        ProcessAuthResults();
    }

}

void ChannelServer::OnAccept()
{
    while(true)
    {
        sockaddr_in caddr{};
        socklen_t clen = sizeof(caddr);

        int cfd = accept(m_listen_fd, reinterpret_cast<sockaddr*>(&caddr), &clen);

        if(cfd < 0)
        {
            // EAGAIN이랑 EWOULDBLOCK은 논블로킹 소켓에서 거의 같은 의미로 본다.
            // EAGAIN : 지금은 자원이 준비되지 않았으니 나중에 다시 시도하라는 의미
            // EWOULDBLOCK : 지금 이 작업을 수행하면 블로킹되는 상태라는 의미
            // 즉, 논블로킹 모드에서는 기다리지 않고 즉시 반환하면서, 지금 당장은 처리할 수 없음을 알려주는 값
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            break;
        }

        if(SetNonblocking(cfd) < 0)
        {
            close(cfd);
            continue;
        }

        unsigned int currentUserCount = m_current_user_count.load();
        if (currentUserCount >= m_max_user_count)
        {
            K_slog_trace(K_SLOG_ERROR, "[%s][%d] Max user count reached. current:%u max:%u fd:%d",
                         __FUNCTION__, __LINE__, currentUserCount, m_max_user_count, cfd);
            close(cfd);
            continue;
        }

        /*
        이 Nagle 알고리즘이라는 것이 작은 데이터를 모아서 한번에 보내는 방식인데

        이게 켜져 있으면 게임 같은 많은 데이터를 보내는 곳에서는 딜레이를 유발할 수있다
        그리고 기본으로 설정되어 있어서 꺼주는게 좋고
        서버 리슨 fd랑 클라이언트 fd 두 곳 모두 설정해줘야 정상적으로 작동한다
        */
        int nodelay = 1;
        if (setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay)) < 0)
        {
            K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] cfd Setsockopt Error \n", __FILE__, __FUNCTION__, __LINE__);
            close(m_listen_fd);
            m_listen_fd =-1;
            close(cfd);
            continue;
        }

        epoll_event cev{};
        // EPOLLIN : 읽기 가능한 상태
        // EPOLLRDHUP : 상대방이 연결의 읽기 방향을 닫았음
        // 이 두개를 같이 설정하는 이유는 클라이언트가 데이터를 보냈거나 연결 종료를 했을 경우 두 경우을 알기 위해서 두 개로 설정
        cev.events = EPOLLIN | EPOLLRDHUP;
        cev.data.fd = cfd;

        if(epoll_ctl(m_epfd, EPOLL_CTL_ADD, cfd, &cev) < 0)
        {
            close(cfd);
            continue;
        }

        uint64_t sessionId = m_nextSessionId.fetch_add(1);
        uint64_t generation = 1;

        ChannelSession* session =  new ChannelSession(cfd, this, sessionId, generation);

        {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            m_sessions[cfd] = session;
        }

        {
            char ip[64]{};
            inet_ntop(AF_INET, &caddr.sin_addr, ip, sizeof(ip));
            // 로그로 접속한 IP 정보 출력
        }
        currentUserCount = m_current_user_count.fetch_add(1) + 1;
        K_slog_trace(K_SLOG_TRACE, "[%s][%d] New Connection FD [%d] Current User Count [%u]\n", __FUNCTION__, __LINE__, cfd, currentUserCount);
    }
}

void ChannelServer::OnReceive(int fd)
{
    char temp[BUFFER_SIZE];
    int tempLen = 0;
    std::string buf;

    do
    {
        memset(temp, 0x00, sizeof(temp));
        tempLen = recv(fd, temp, sizeof(temp), 0);

        if (tempLen > 0)
        {
            buf.append(temp, tempLen);
        }
        else if (tempLen == 0)
        {
            // 진짜 클라이언트 종료
            OnDisconnect(fd);
            return;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; // 정상. 지금 읽을 데이터 없음
        
            // 진짜 recv 에러
            OnDisconnect(fd);
            return;
        }
    } while (tempLen == BUFFER_SIZE);

    //K_slog_trace(K_SLOG_DEBUG, "fd %d\n", fd);
    ChannelSession* session = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);

        auto it = m_sessions.find(fd);
        if(it == m_sessions.end())
            return;

        session = it->second;
    }
    if(!session->OnBytes((const uint8_t*)buf.c_str(), buf.size()))
    {
        OnDisconnect(fd);
    }
}

ChannelSession* ChannelServer::FindValidSession(int fd, uint64_t sessionId, uint64_t generation)
{
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    auto it = m_sessions.find(fd);
    if (it == m_sessions.end())
        return nullptr;

    ChannelSession* session = it->second;
    if (session == nullptr)
        return nullptr;

    if (session->GetSessionId() != sessionId)
        return nullptr;

    if (session->GetGeneration() != generation)
        return nullptr;

    if (session->IsClosing())
        return nullptr;

    return session;
}

ChannelSession* ChannelServer::BeginValidSessionTask(int fd, uint64_t sessionId, uint64_t generation)
{
    std::lock_guard<std::mutex> lock(m_sessionMutex);
    auto it = m_sessions.find(fd);
    if (it == m_sessions.end())
        return nullptr;

    ChannelSession* session = it->second;
    if (session == nullptr)
        return nullptr;

    if (session->GetSessionId() != sessionId)
        return nullptr;

    if (session->GetGeneration() != generation)
        return nullptr;

    if (!session->TryBeginTask())
        return nullptr;

    return session;
}

void ChannelServer::EndSessionTask(ChannelSession* session)
{
    if (session != nullptr)
        session->EndTask();
}

void ChannelServer::OnDisconnect(int fd)
{
   ChannelSession* session = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_sessionMutex);

        auto it = m_sessions.find(fd);
        if(it == m_sessions.end())
            return;

        session = it->second;
        session->MarkClosing();
        m_sessions.erase(it);
    }

    epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
    session->WaitForNoTasks();
    delete session;

    unsigned int currentUserCount = m_current_user_count.load();
    while (currentUserCount > 0 &&
           !m_current_user_count.compare_exchange_weak(currentUserCount, currentUserCount - 1))
    {
    }
}


// 클라이언트 fd는 기본적으로 읽기/끊김 이벤트만 감시한다.
// 송신 큐에 보낼 데이터가 생기면 EPOLLOUT을 추가해서,
// 소켓이 쓰기 가능한 시점에 남은 데이터를 이어서 전송한다.
// 송신 큐에 대기 중인 데이터가 있을 때 EPOLLOUT 감시를 켠다.
// EPOLLOUT은 계속 켜두면 반복적으로 발생하므로, FlushSend 완료 후 다시 끈다.
void ChannelServer::EnableWriteEvent(int fd)
{
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLRDHUP | EPOLLOUT;

    if (epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &ev) < 0)
    {
        K_slog_trace(
            K_SLOG_ERROR,"[%s : %s : %d] EPOLL_CTL_MOD enable write failed fd:%d errno:%d",__FILE__, __FUNCTION__, __LINE__, fd, errno);
    }
}

void ChannelServer::PushAuthResult(ChannelAuthResult result)
{
    std::lock_guard<std::mutex> lock(m_authResultMutex);
    m_authResults.push(std::move(result));
}

void ChannelServer::UpdateChannelState(const int interval, const int ttl)
{
    while(true)
    {
        auto task = std::make_unique<ChannelStateUpdateTask>(this, ttl);
        this->GetThreadPool()->Submit(std::move(task));
        sleep(interval); // 지정된 시간마다 업데이트
    }
}

void ChannelServer::UpdateChannelStateToRedis(const int ttl)
{
    RedisConnectionGuard redisGuard(&m_redisPool);
    RedisClient* redis = redisGuard.Get();
    int curUser = static_cast<int>(m_current_user_count.load());
    int maxUser = m_max_user_count;
    std::string state;
    int rc;

    if (redis == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] RedisClient is nullptr", __FUNCTION__, __LINE__);
        return;
    }

    int percentage = 0;
    if (maxUser > 0)
    {
        percentage = (curUser * 100) / maxUser;
    }
    percentage = std::min(percentage, 100); // 최대 100%로 제한

    if (percentage >= 90)
    {
        state = ChannelState::FULL;
    }
    else if (percentage >= 70)
    {
        state = ChannelState::BUSY;
    }
    else
    {
        state = ChannelState::NORMAL;
    }

    
    std::map<std::string, std::string> redisMap;
    redisMap["state"] = state;
    redisMap["percentage"] = std::to_string(percentage);

    rc = redis->HSetAll("channel:" + std::to_string(m_channel_id) + ":status", redisMap, ttl); // 지정된 시간 동안 유지
    if (rc != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] Failed to update channel status to Redis", __FUNCTION__, __LINE__);
        return;
    }

    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] Updated channel status to Redis: state=%s, percentage=%d%%", __FUNCTION__, __LINE__, state.c_str(), percentage);
    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] Current User Count: %d, Max User Count: %d", __FUNCTION__, __LINE__, curUser, maxUser);
}

void ChannelServer::ProcessAuthResults()
{
    std::queue<ChannelAuthResult> local;

    {
        std::lock_guard<std::mutex> lock(m_authResultMutex);
        std::swap(local, m_authResults);
    }

    while (!local.empty())
    {

        ChannelAuthResult result = std::move(local.front());
        local.pop();
        ChannelSession* session = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_sessionMutex);
            auto it = m_sessions.find(result.fd);
            if (it == m_sessions.end())
            {
                K_slog_trace(K_SLOG_TRACE, "[ProcessAuthResults] fd closed:%d", result.fd);
                continue;
            }

            session = it->second;
        }

        if (!result.success || result.player == nullptr)
        {
            session->SendNok(PKT_CHANNEL_AUTH, result.error.empty() ? "auth failed" : result.error);
            continue;
        }

        Player* rawPlayer = result.player.get();
        rawPlayer->SetSession(session);

        if (!m_player_mamager.AddPlayer(std::move(result.player)))
        {   
            session->SendNok(PKT_CHANNEL_AUTH, "already connected");
            continue;
        }

        session->SetPlayer(rawPlayer);
        session->SetPlayerManager(&m_player_mamager);

        session->SendOk(PKT_CHANNEL_AUTH, { rawPlayer->GetName() });

        PlayerPacketSender::SendPlayerInfo(rawPlayer);
        PlayerPacketSender::SendPlayerStat(rawPlayer);
        PlayerPacketSender::SendPlayerSkillList(rawPlayer);

        InventoryPacketSender::SendInventoryMeta(rawPlayer);
        InventoryPacketSender::SendInventoryItems(rawPlayer);

        QuickSlotPacketSender::SendQuickSlotList(rawPlayer);
    }
}
