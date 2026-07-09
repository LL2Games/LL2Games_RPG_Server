#include "ChannelManager.h"
#include "K_slog.h"
#include "RedisClient.h"
#include "MySqlConnectionPool.h"
#include "RedisCommonEnum.h"

ChannelManager::ChannelManager()
{
}
ChannelManager::~ChannelManager()
{
}

// static void LogChannelInfo(ChannelInfo info)
// {
//     K_LOG_DEBUG( "--------------------------");
//     K_LOG_DEBUG( "channel_id: %s", info.channel_id.c_str());
//     K_LOG_DEBUG( "ip: %s", info.ip.c_str());
//     K_LOG_DEBUG( "port: %d", info.port);
//     K_LOG_DEBUG( "max_users: %d", info.max_users);
//     K_LOG_DEBUG( "current_users: %d", info.current_users);
//     K_LOG_DEBUG( "alive: %d", info.alive);
//     K_LOG_DEBUG( "last_heartbeat: %s", info.last_heartbeat.c_str());
//     K_LOG_DEBUG( "--------------------------\r\n");
// }

int ChannelManager::Init()
{
    int rc = EXIT_SUCCESS;
    MYSQL* conn = nullptr;
    std::string query;
    const std::string channel_id = "ch_01"; //입력받는 로직 또는 BestChannel정하는 로직으로 변경예정

    conn = MySqlConnectionPool::GetInstance()->GetConnection();
    if (!conn)
    {
        K_LOG_ERROR( "MYSQL GetConnection failed");
        goto err; 
    }

    //channel_id, ip, port, max_users, current_users, alive, last_heartbeat
    query = std::string("SELECT * FROM channel");
    K_LOG_DEBUG( "query[%s]", query.c_str());

    if (mysql_query(conn, query.c_str()) == 0)
    {
        MYSQL_RES* res = mysql_store_result(conn);
        if (!res)
        {
            K_LOG_ERROR( "mysql_store_result failed: %s", mysql_error(conn));
            rc = EXIT_FAILURE;
            goto err;
        }

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)))
        {
            ChannelInfo info;
            info.channel_id = row[0];
            info.ip = row[1];
            info.port = atoi(row[2]);
            info.max_users = atoi(row[3]);
            info.current_users = atoi(row[4]);
            info.alive = atoi(row[5]);
            info.last_heartbeat = row[6];

            m_channels[info.channel_id] = info;
            //LogChannelInfo(info);
            //Redis캐시 적재
        }

        mysql_free_result(res);        
    }

err:
    if (conn)
    {
        MySqlConnectionPool::GetInstance()->ReleaseConnection(conn);
        conn = nullptr;
    }

    return rc;
}

std::optional<ChannelInfo> ChannelManager::SelectChannel(const std::string &channel_id)
{
    ChannelInfo info;
    auto it = m_channels.find(channel_id);
    if (it == m_channels.end())
    {
        K_LOG_ERROR( "channel(%s) is none", channel_id.c_str());
        return std::nullopt;
    }
    info = it->second;
    if (info.alive == 0)
    {
        K_LOG_ERROR( "channel(%s) is die", channel_id);
        return std::nullopt;
    }

    return info;
}

ChannelInfo ChannelManager::SelectBestChannel()
{
    ChannelInfo info;
    // MYSQL* conn = nullptr;
    // std::string query;
    // const std::string channel_id = "ch_01"; //입력받는 로직 또는 BestChannel정하는 로직으로 변경예정
    // conn = MySqlConnectionPool::GetInstance()->GetConnection();
    // if (!conn)
    // {
    //     K_LOG_ERROR( "MYSQL GetConnection failed");
    //     goto err; 
    // }

    // //channel_id, ip, port, max_users, current_users, alive, last_heartbeat
    // query = std::string("SELECT ip, port, alive FROM channel WHERE channel_id='" + channel_id + "'");
    // K_LOG_DEBUG( "query[%s]", query.c_str());

    // if (mysql_query(conn, query.c_str()) == 0)
    // {
    //     MYSQL_RES* res = mysql_store_result(conn);
    //     MYSQL_ROW row;
    //     info.ip = row[0];
    //     info.port = atoi(row[1]);
    //     info.alive = atoi(row[2]);
    // }

//err:
    return info;
}

int ChannelManager::CanEnterChannel(const std::string& channel_id, RedisClient& redis)
{
    E_ChannelState result_state = E_ChannelState::Die;
   

    auto redis_value = redis.HGetAll("channel:" + channel_id + ":status");
    if (redis_value.has_value() && !redis_value->empty())
    {
        auto it = redis_value->find("state");
        if (it != redis_value->end())
        {
            std::string state = it->second;
            if (state == ChannelState::NORMAL)
                result_state = E_ChannelState::Normal;
            else if (state == ChannelState::BUSY)
                result_state = E_ChannelState::Busy;
            else if (state == ChannelState::FULL)
                result_state = E_ChannelState::Full;
            else 
                result_state = E_ChannelState::Die;

            K_LOG_DEBUG( "channel(%s) status from Redis: state=%s", channel_id.c_str(), state.c_str());
            K_LOG_DEBUG( "channel(%s) percentage from Redis: percentage=%s%%", channel_id.c_str(), redis_value->find("percentage") != redis_value->end() ? redis_value->find("percentage")->second.c_str() : "N/A");
        }
    }
    else
    {
        K_LOG_ERROR( "Failed to get channel status from Redis for channel(%s)", channel_id.c_str());
        return (int)E_ChannelState::Die;
    }

    return (int)result_state;
}