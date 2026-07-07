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
            K_slog_trace(K_SLOG_TRACE, "==============LOG_LEVEL: %d NO LOG==============", g_config.common.logLevel);
            K_slog_close();
        }
        K_slog_init(CHANNEL_LOG_PATH, daemonName.c_str(), g_config.common.logLevel);
        K_slog_trace(K_SLOG_TRACE, "[%s]==============START==============", daemonName.c_str());
        K_slog_trace(K_SLOG_TRACE, "[%s]==============LOG_LEVEL: %d==============", daemonName.c_str(), g_config.common.logLevel);
        if (MySqlConnectionPool::Init(g_config.mysql, g_config.mysql.poolCount) != EXIT_SUCCESS)
        {
            K_slog_trace(K_SLOG_ERROR, "Failed to init MySqlConnectionPool");
            K_slog_close();
            return -1;
        }

        K_slog_trace(K_SLOG_TRACE, "[%s]==============MySqlConnectionPool Count: %d==============", daemonName.c_str(), MySqlConnectionPool::GetInstance()->GetPoolSize());
        //if (RedisClient::Init(g_config.redis) != EXIT_SUCCESS)
        //{
        //    K_slog_trace(K_SLOG_ERROR, "Failed to init RedisClient");
        //    K_slog_close();
        //    return -1;
        //}
        ChannelServer channelServer(channelIndex + 1, g_config.channelServer.threadCount, g_config.channelServer.maxUserCount); // 채널 인덱스는 1부터 시작

        bool start = channelServer.Init(g_config.channelServer.port + channelIndex, g_config.redis);
        if (start == false)
        {
            K_slog_close();
            return -1;
        }

        channelServer.Run();

        K_slog_trace(K_SLOG_TRACE, "[%s]..................the End..............", daemonName.c_str());
        K_slog_close();
    }
    catch (const std::exception& ex)
    {
        printf("[%s] Exception: %s\n", daemonName.c_str(), ex.what());
        K_slog_trace(K_SLOG_ERROR, "Exception: %s", ex.what());
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
    K_slog_trace(K_SLOG_TRACE, "[%s]==============START==============", CHANNEL_DAEMON_NAME);

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

    K_slog_trace(K_SLOG_TRACE, "[%s]..................the End..............", CHANNEL_DAEMON_NAME);
    K_slog_close();
    return 0;
}
#endif