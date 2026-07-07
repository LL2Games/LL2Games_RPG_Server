#include "common.h"
#include "Server.h"
#include "MySqlConnectionPool.h"
#include "RedisClient.h"
#include "MySQLManager.h"
#include "ConfigLoader.h"


#if 1
namespace
{
    AppConfig g_config;
}

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
            K_slog_init(LOGIN_LOG_PATH, LOGIN_DAEMON_NAME, 1);
            K_slog_trace(K_SLOG_TRACE, "==============LOG_LEVEL: %d NO LOG==============", g_config.common.logLevel);
            K_slog_close();
        }
        K_slog_init(LOGIN_LOG_PATH, LOGIN_DAEMON_NAME, g_config.common.logLevel);
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
        bool start = server.Init(g_config.loginServer.port, g_config.redis);
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
        printf("[%s] Exception: %s\n", LOGIN_DAEMON_NAME, ex.what());
        K_slog_trace(K_SLOG_ERROR, "Exception: %s", ex.what());
        K_slog_close();
        return -1;
    }
    return 0;
}

#else
int main()
{
    // log
    K_slog_init(LOGIN_LOG_PATH, LOGIN_DAEMON_NAME);
    K_slog_trace(K_SLOG_TRACE, "[%s]==============START==============", LOGIN_DAEMON_NAME);

    MySQLManager::Instance().Connect("127.0.0.1", "root", "1234", "game", 3306);

    Server server;
    if (!server.Init(PORT))
        return -1;

    server.Run();

    K_slog_trace(K_SLOG_TRACE, "[%s]..................the End..............", LOGIN_DAEMON_NAME);
    K_slog_close();
    return 0;
}
#endif