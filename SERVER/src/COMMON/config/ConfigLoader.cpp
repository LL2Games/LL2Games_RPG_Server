#include "ConfigLoader.h"
#include <fstream>
#include <stdexcept>

bool ConfigLoader::Load(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return false;
    }

    std::string currentSection;
    std::string line;

    while (std::getline(file, line))
    {
        line = Trim(line);

        if (line.empty())
        {
            continue;
        }

        if (line[0] == '#' || line[0] == ';')
        {
            continue;
        }

        if (line.front() == '[' && line.back() == ']')
        {
            currentSection = Trim(line.substr(1, line.size() - 2));
            continue;
        }

        size_t equalPos = line.find('=');
        if (equalPos == std::string::npos)
        {
            continue;
        }

        std::string key = Trim(line.substr(0, equalPos));
        std::string value = Trim(line.substr(equalPos + 1));

        if (!currentSection.empty() && !key.empty())
        {
            values_[currentSection][key] = value;
        }
    }

    return true;
}

AppConfig ConfigLoader::ToAppConfig() const
{
    AppConfig appConfig;

    appConfig.mysql.host = GetString("MYSQL", "HOST");
    appConfig.mysql.port = GetInt("MYSQL", "PORT");
    appConfig.mysql.user = GetString("MYSQL", "USER");
    appConfig.mysql.password = GetString("MYSQL", "PASSWORD");
    appConfig.mysql.database = GetString("MYSQL", "DATABASE");
    
    appConfig.redis.host = GetString("REDIS", "HOST");
    appConfig.redis.port = GetInt("REDIS", "PORT");
    
    appConfig.loginServer.port = GetInt("LOGIN_SERVER", "PORT");
    appConfig.worldServer.port = GetInt("WORLD_SERVER", "PORT");
    appConfig.channelServer.port = GetInt("CHANNEL_SERVER", "PORT");
    appConfig.chatServer.port = GetInt("CHAT_SERVER", "PORT");
    
    try {appConfig.mysql.poolCount = GetInt("MYSQL", "POOL_COUNT");} catch (const std::exception&) {printf("MYSQL POOL_COUNT not found, using default value"); appConfig.mysql.poolCount = 8;} 

    try {appConfig.redis.poolCount = GetInt("REDIS", "POOL_COUNT");} catch (const std::exception&) {printf("REDIS POOL_COUNT not found, using default value"); appConfig.redis.poolCount = 8;}

    try {appConfig.channelServer.maxUserCount = GetInt("CHANNEL_SERVER", "MAX_USER_COUNT");} catch (const std::exception&) {printf("CHANNEL_SERVER MAX_USER_COUNT not found, using default value"); appConfig.channelServer.maxUserCount = 1000;} 
    
    try {appConfig.channelServer.threadCount = GetInt("CHANNEL_SERVER", "THREAD_COUNT");} catch (const std::exception&) {printf("CHANNEL_SERVER THREAD_COUNT not found, using default value"); appConfig.channelServer.threadCount = 0;} 

    try {appConfig.common.logLevel = GetInt("COMMON", "LOG_LEVEL");} catch (const std::exception&) {printf("COMMON LOG_LEVEL not found, using default value"); appConfig.common.logLevel = 2;}

    return appConfig;
}

std::string ConfigLoader::GetString(const std::string &section, const std::string &key) const
{
    auto sectionIt = values_.find(section);
    if(sectionIt == values_.end())
    {
        throw std::runtime_error("Config Section을 찾지 못했습니다: " + section);
    }

    auto keyIt = sectionIt->second.find(key);
    if(keyIt == sectionIt->second.end())
    {
        throw std::runtime_error("Config key를 찾지 못했습니다: " + section + "." + key);
    }

    return keyIt->second;
}

int ConfigLoader::GetInt(const std::string &section, const std::string &key) const
{
    return std::stoi(GetString(section, key));
}

std::string ConfigLoader::Trim(const std::string &value)
{
    const char* whitespace = " \t\r\n";

    size_t begin = value.find_first_not_of(whitespace);
    if(begin == std::string::npos)
    {
        return "";
    }

    size_t end = value.find_last_not_of(whitespace);
    return value.substr(begin, end- begin + 1);
}
