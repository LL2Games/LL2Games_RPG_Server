#pragma once

#include "common.h"
#include "CommonEnum.h"
#include "MapInstance.h"
#include <nlohmann/json.hpp>
#include <filesystem> //C++ 17 g++ 사용의 경우 8버전 이상이 설치되어 있어야 한다. 아니면 <experimental/filesystem>을 사용해야 한다.
#include <fstream>
#include <queue>
#include "MapUpdateTask.h"
#include <thread>
#include <atomic>
#include <mutex>

class ChannelServer;

class MapManager
{
public:
    MapManager(ChannelServer *server);
    ~MapManager();

    bool Init();
    MapInstance* GetOrCreate(int mapId);
    bool PreLoadAll();
    void LoadMonster(nlohmann::json& j, std::vector<MonsterSpawnData>& MonstersData);
    bool LoadJsonFile(int mapId, MapInitData& mapData);
    void RemoveMap();

    
private:
    // key : map_id value : map
    std::unordered_map<int, MapInstance*> m_maps;
    std::unordered_map<int, MapInitData> m_maps_initData;
    std::queue<uint16_t> m_destroyQueue;
    ChannelServer* m_server;


//Thread
public:
    void Update();
    void Start();
    void Stop();
private:
    std::atomic<bool> m_running{false};
    std::mutex m_mapsMutex;
    std::mutex m_destroyQueueMutex;
    std::thread m_thread;
};
