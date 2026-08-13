#include <atomic>
#include <csignal>
#include <exception>
#include <pthread.h>
#include <thread>
#include <cerrno>

#include "common.h"
#include "ChannelServer.h"
#include "ConfigLoader.h"
#if 1 /*DB 연결 테스트*/
#include "MySqlConnectionPool.h"
#include "ItemManager.h"
#include "MapManager.h"
#endif



#if 1
namespace
{
    AppConfig g_config;
}

int main(int ac, char **av)
{
    std::string daemonName = CHANNEL_DAEMON_NAME;
    try
    {
        int channelIndex = 0;
        std::string configPath;

        for (int i = 1; i < ac; ++i)
        {
            std::string arg = av[i];

            if (arg == "--config")
            {
                if (i + 1 >= ac)
                {
                    printf("Missing config path after --config");
                    return -1;
                }

                configPath = av[++i];
            }
            else
            {
                channelIndex = std::atoi(arg.c_str());
                daemonName += "_" + std::to_string(channelIndex + 1); // 채널 인덱스는 1부터 시작
            }
        }

        if (configPath.empty())
        {
            printf("Missing required --config argument");
            return -1;
        }

        ConfigLoader loader;
        if (!loader.Load(configPath))
        {
            printf("Failed to load config: %s", configPath.c_str());
            return -1;
        }

        g_config = loader.ToAppConfig();
        if (g_config.common.logLevel == 0)
        {
            K_slog_init(CHANNEL_LOG_PATH, daemonName.c_str(), 1);
            K_LOG_TRACE( "==============LOG_LEVEL: %d NO LOG==============", g_config.common.logLevel);
            K_slog_close();
        }
        K_slog_init(CHANNEL_LOG_PATH, daemonName.c_str(), g_config.common.logLevel);
        K_LOG_TRACE( "[%s]==============START==============", daemonName.c_str());
        K_LOG_TRACE( "[%s]==============LOG_LEVEL: %d==============", daemonName.c_str(), g_config.common.logLevel);
        if (MySqlConnectionPool::Init(g_config.mysql, g_config.mysql.poolCount) != EXIT_SUCCESS)
        {
            K_LOG_ERROR( "Failed to init MySqlConnectionPool");
            K_slog_close();
            return -1;
        }

        K_LOG_TRACE( "[%s]==============MySqlConnectionPool Count: %d==============", daemonName.c_str(), MySqlConnectionPool::GetInstance()->GetPoolSize());
        //if (RedisClient::Init(g_config.redis) != EXIT_SUCCESS)
        //{
        //    K_LOG_ERROR( "Failed to init RedisClient");
        //    K_slog_close();
        //    return -1;
        //}
        // SIGINT와 SIGTERM 종료 신호를 일반 시그널 핸들러에서 바로 처리하지 않고, 별도 스레드가 안전하게 기다려 처리하도록 먼저 차단
        sigset_t stopSignals{};
        // 처리할 시그널 목록을 담는 변수
        sigemptyset(&stopSignals);
        // 기다릴 종료 신호를 목록에 추가
        sigaddset(&stopSignals, SIGINT); // 터미널에서 Ctrl+C
        sigaddset(&stopSignals, SIGTERM); // kill <pid>, 프로세스 관리자 등의 정상 종료 요청

        // 현재 스레드에서 SIGINT와 SINGTERM의 자동 처리를 방지
        const int maskResult = pthread_sigmask(SIG_BLOCK,&stopSignals,nullptr);

        if (maskResult != 0)
        {
            K_LOG_ERROR("Failed to block termination signals. error[%d]",maskResult);
            K_slog_close();
            return -1;
        }
        ChannelServer channelServer(channelIndex + 1, g_config.channelServer.threadCount, g_config.channelServer.maxUserCount); // 채널 인덱스는 1부터 시작

        bool start = channelServer.Init(g_config.channelServer.port + channelIndex, g_config.redis);
        if (start == false)
        {
            K_slog_close();
            return -1;
        }

        // 종료 신호를 기다리는 스레드의 종료 여부
        std::atomic<bool> stopSignalThread{false};

    // SIGINT 또는 SIGTERM을 기다리는 전용 스레드
        std::thread signalThread([&channelServer, &stopSignals, &stopSignalThread]()
        {
            while (!stopSignalThread.load(std::memory_order_acquire))
            {
                // Run()이 신호 이외의 이유로 끝났을 때도
                // 이 스레드를 종료할 수 있도록 1초 단위로 확인한다.
                timespec timeout{};
                timeout.tv_sec = 1;
                timeout.tv_nsec = 0;

                const int signalNumber = sigtimedwait(&stopSignals,nullptr,&timeout);

                if (signalNumber == SIGINT || signalNumber == SIGTERM)
                {
                    K_LOG_TRACE("Termination signal received. signal[%d]",signalNumber);
                    channelServer.RequestStop();
                    return;
                }

                if (signalNumber == -1)
                {
                    // 제한 시간 만료 또는 인터럽트는 정상적으로 다시 확인한다.
                    if (errno == EAGAIN || errno == EINTR)
                    {
                        continue;
                    }   
                    K_LOG_ERROR("Failed to wait for termination signal. errno[%d]",errno);
                    channelServer.RequestStop();
                    return;
                }
            }
        }
    );

        std::exception_ptr runException;

        try
        {
            channelServer.Run();
        }
        catch (...)
        {
            // signalThread를 먼저 정리한 뒤 예외를 다시 전달하기 위해 보관한다.
            runException = std::current_exception();
        }

        // Run()이 신호 이외의 이유로 종료된 경우 signalThread도 종료한다.
        stopSignalThread.store(true, std::memory_order_release);

        if (signalThread.joinable())
        {
            signalThread.join();
        }

        if (runException != nullptr)
        {
            std::rethrow_exception(runException);
        }
        
        K_LOG_TRACE( "[%s]..................the End..............", daemonName.c_str());
        K_slog_close();
        
    }
    catch (const std::exception& ex)
    {
        printf("[%s] Exception: %s\n", daemonName.c_str(), ex.what());
        K_LOG_ERROR( "Exception: %s", ex.what());
        K_slog_close();
        return -1;
    }

    return 0;
}
#else
int main(int ac, char **av)
{
    // log
    K_slog_init(CHANNEL_LOG_PATH, CHANNEL_DAEMON_NAME);
    K_LOG_TRACE( "[%s]==============START==============", CHANNEL_DAEMON_NAME);

    bool Start = false;

    ChannelServer channelServer;

    if (ac == 2)
    {
        Start = channelServer.Init(PORT + atoi(av[1]));
    }
    else
        Start = channelServer.Init(PORT);

    if (Start == false)
    {
        return -1;
    }

    channelServer.Run();

    K_LOG_TRACE( "[%s]..................the End..............", CHANNEL_DAEMON_NAME);
    K_slog_close();
    return 0;
}
#endif