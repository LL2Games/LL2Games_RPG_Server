#include "Server.h"
#include "MySqlConnectionPool.h"
#include "ConfigLoader.h"

#include "common.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <exception>
#include <pthread.h>
#include <thread>

#if 1
namespace
{
    AppConfig g_config;
}

int main(int ac, char **av)
{
    std::string daemonName = CHAT_DAEMON_NAME;
    try
    {
        int chatIndex = 0;
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
                chatIndex = std::atoi(arg.c_str());
                daemonName += "_" + std::to_string(chatIndex + 1); // 채널 인덱스는 1부터 시작
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
            K_slog_init(CHAT_LOG_PATH, daemonName.c_str(), 1);
            K_LOG_TRACE( "==============LOG_LEVEL: %d NO LOG==============", g_config.common.logLevel);
            K_slog_close();
        }
        K_slog_init(CHAT_LOG_PATH, daemonName.c_str(), g_config.common.logLevel);
        K_LOG_TRACE( "==============START==============");
        K_LOG_TRACE( "==============LOG_LEVEL: %d==============", g_config.common.logLevel);

        if (MySqlConnectionPool::Init(g_config.mysql, g_config.mysql.poolCount) != EXIT_SUCCESS)
        {
            K_LOG_ERROR( "Failed to init MySqlConnectionPool");
            K_slog_close();
            return -1;
        }
        K_LOG_TRACE( "==============MySqlConnectionPool Count: %d==============", MySqlConnectionPool::GetInstance()->GetPoolSize());
       
        sigset_t stopSignals{};

        sigemptyset(&stopSignals);
        sigaddset(&stopSignals, SIGINT);
        sigaddset(&stopSignals, SIGTERM);

        const int maskResult = pthread_sigmask(SIG_BLOCK, &stopSignals, nullptr);

        if(maskResult != 0)
        {
            K_LOG_ERROR("Failed to block termination signals, errno[%d]", maskResult);

            K_slog_close();
            return -1;
        }
        Server server;

        bool start = server.Init(g_config.chatServer.port + chatIndex, g_config.redis);
        if (start == false)
        {
            K_slog_close();
            return -1;
        }

        std::atomic_bool stopSignalsThread{false};

        std::thread signalThread([&server, &stopSignals, &stopSignalsThread]()
        {
            while(!stopSignalsThread.load(std::memory_order_acquire))
            {
                timespec timeout{};
                timeout.tv_sec = 1;
                timeout.tv_nsec = 0;

                const int signalNumber = sigtimedwait(&stopSignals, nullptr, &timeout);

                if(signalNumber == SIGINT || signalNumber == SIGTERM)
                {
                    K_LOG_TRACE("[CHAT] Termination signal received, signal [%d]", signalNumber);

                    server.RequestStop();
                    return;
                }

                if(signalNumber == -1)
                {
                    if(errno == EAGAIN || errno == EINTR)
                        continue;

                    K_LOG_ERROR("[CHAT] Failed to wait for termination signal, errno[%d]", errno);

                    server.RequestStop();
                    return;
                }
            }   
            
        });

        std::exception_ptr runException;

        try
        {
            server.Run();
        }
        catch (...)
        {
            // signalThread를 먼저 정리하기 위해 예외를 보관한다.
            runException = std::current_exception();
        }

        // Run()이 시그널 외의 이유로 끝났을 때
        // 시그널 대기 스레드도 종료한다.
        stopSignalsThread.store(true, std::memory_order_release);

        if (signalThread.joinable())
        {
            signalThread.join();
        }

        // Run 루프가 완전히 끝난 뒤 메인 스레드에서
        // 종료 패킷 전송 및 세션 정리를 실행한다.
        server.ShutdownGracefully();

        MySqlConnectionPool::Shutdown();

        if (runException != nullptr)
        {
            std::rethrow_exception(runException);
        }

        K_LOG_TRACE("..................the End..............");
        K_slog_close();
    }
    catch (const std::exception &ex)
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
    K_slog_init(CHAT_LOG_PATH, "CHAT_SERVER");
    K_LOG_TRACE( "[%s]==============START==============", CHAT_DAEMON_NAME);

    MySQLManager::Instance().Connect("127.0.0.1", "root", "1234", "game", 3306);

    Server server;

    if (ac == 2)
    {
        if (!server.Init(PORT + atoi(av[1])))
            return -1;
    }
    else
    {
        if (!server.Init(PORT))
            return -1;
    }

    server.Run();
    K_LOG_TRACE( "[%s]..................the End..............", CHAT_DAEMON_NAME);
    K_slog_close();
    return 0;
}
#endif