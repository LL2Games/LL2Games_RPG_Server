#pragma once
#include <map>
#include <string>
#include <optional>
#include "RedisClient.h"

struct ChannelInfo
{
    std::string channel_id;
    std::string ip;
    int port;
    int max_users;
    int current_users;
    int alive;
    std::string last_heartbeat;
};

class ChannelManager
{
public:
    ChannelManager();
    ~ChannelManager();

    int Init();
    int AddOrUpdateChannel(const std::string info);

    std::optional<ChannelInfo> SelectChannel(const std::string &channel_id);
    ChannelInfo SelectBestChannel();

    ChannelInfo* GetChannel(const std::string& id);
    int LoadFromRedis(RedisClient&);
    int SaveToRedis(RedisClient&);
    int CanEnterChannel(const std::string& channel_id, RedisClient& redis);
private:
    //key=channel_id, value=ChannelInfo
    std::map<std::string, ChannelInfo> m_channels;
};