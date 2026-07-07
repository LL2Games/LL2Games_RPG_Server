#include "Server.h"
#include "MySqlConnectionPool.h"
#include "ConfigLoader.h"

#include "common.h"

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
            K_slog_trace(K_SLOG_TRACE, "==============LOG_LEVEL: %d NO LOG==============", g_config.common.logLevel);
            K_slog_close();
        }
        K_slog_init(CHAT_LOG_PATH, daemonName.c_str(), g_config.common.logLevel);
        K_slog_trace(K_SLOG_TRACE, "==============START==============");
        K_slog_trace(K_SLOG_TRACE, "==============LOG_LEVEL: %d==============", g_config.common.logLevel);

        if (MySqlConnectionPool::Init(g_config.mysql, g_config.mysql.poolCount) != EXIT_SUCCESS)
        {
            K_slog_trace(K_SLOG_ERROR, "Failed to init MySqlConnectionPool");
            K_slog_close();
            return -1;
        }
        K_slog_trace(K_SLOG_TRACE, "==============MySqlConnectionPool Count: %d==============", MySqlConnectionPool::GetInstance()->GetPoolSize());
       
        Server server;

        bool start = server.Init(g_config.chatServer.port + chatIndex, g_config.redis);
        if (start == false)
        {
            K_slog_close();
            return -1;
        }

        server.Run();

        K_slog_trace(K_SLOG_TRACE, "..................the End..............");
        K_slog_close();
    }
    catch (const std::exception &ex)
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
    K_slog_init(CHAT_LOG_PATH, "CHAT_SERVER");
    K_slog_trace(K_SLOG_TRACE, "[%s]==============START==============", CHAT_DAEMON_NAME);

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
    K_slog_trace(K_SLOG_TRACE, "[%s]..................the End..............", CHAT_DAEMON_NAME);
    K_slog_close();
    return 0;
}
#endif