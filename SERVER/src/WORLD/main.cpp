#include "common.h"
#include "WorldServer.h"
#include "ConfigLoader.h"
#include "MySqlConnectionPool.h"
#include "RedisClient.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <exception>
#include <pthread.h>
#include <thread>

namespace
{
    AppConfig g_config;
}

#if 1
int main(int ac, char **av)
{
    try
    {
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
            K_slog_init(WORLD_LOG_PATH, WORLD_DAEMON_NAME, 1);
            K_LOG_TRACE( "==============LOG_LEVEL: %d NO LOG==============", g_config.common.logLevel);
            K_slog_close();
        }
        K_slog_init(WORLD_LOG_PATH, WORLD_DAEMON_NAME, g_config.common.logLevel);
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
            K_LOG_ERROR("Failed to block termination signals. error[%d]", errno);

            K_slog_close();
            return -1;
        }


        WorldServer server;

        if (server.Init(g_config.worldServer.port, g_config.redis) != 0)
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
                    K_LOG_TRACE("[WORLD] Termination siganl received. signal [%d]", signalNumber);

                    server.RequestStop();
                    return;
                }

                if(signalNumber == -1)
                {
                    if(errno == EAGAIN || errno == EINTR)
                        continue;

                    K_LOG_ERROR("[WORLD] Failed to wait for termination signal, errno[%d]", errno);

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
        catch(...)
        {
            runException = std::current_exception();
        }

        stopSignalsThread.store(true, std::memory_order_release);

        if(signalThread.joinable())
        {
            signalThread.join();
        }

        server.ShutdownGracefully();

        MySqlConnectionPool::Shutdown();

        if(runException != nullptr)
        {
            std::rethrow_exception(runException);
        }

        K_LOG_TRACE("..................the End..............");
        K_slog_close();
    }
    catch (const std::exception &ex)
    {
        printf("[%s] Exception: %s\n", WORLD_DAEMON_NAME, ex.what());
        K_LOG_ERROR( "Exception: %s", ex.what());
        K_slog_close();
        return -1;
    }
    return 0;
}

#else
int main()
{
    WorldServer server;
    K_slog_init(WORLD_LOG_PATH, WORLD_DAEMON_NAME);
    K_LOG_TRACE( "[%s]==============START==============", WORLD_DAEMON_NAME);
    server.Init(WORLD_PORT);
    server.Run();

    K_LOG_TRACE( "[%s]..................the End..............", WORLD_DAEMON_NAME);
    K_slog_close();
    return 0;
}
#endif