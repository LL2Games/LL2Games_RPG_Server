#include "common.h"
#include "MapManager.h"
#include "PlayerManager.h"
#include "MapUpdateTask.h"
#include "ChannelServer.h"
#include <algorithm>

#define MAP_PATH "../src/CHANNEL/data/Maps/"
namespace fs = std::filesystem;

MapManager::MapManager(ChannelServer *server) : m_server(server)
{
}

MapManager::~MapManager()
{
    Stop();
}

bool MapManager::Init()
{
    // 서버 구동 시 Map 데이터 전부 읽어서 미리 저장
    if(!PreLoadAll()) return false;
    return true;
}

void MapManager::Start()
{
    bool expected = false;

    if (!m_running.compare_exchange_strong(expected, true))
    {
        K_LOG_ERROR("[MapManager] Already started");
        return;
    }

    try
    {
        m_thread = std::thread(&MapManager::Update, this);
    }
    catch (...)
    {
        m_running.store(false);
        throw;
    }

    K_LOG_DEBUG("MapManager Update Start");
}

void MapManager::Stop()
{
    {
        std::lock_guard<std::mutex> lock(m_updateWaitMutex);
        m_running.store(false, std::memory_order_release);
    }

    m_updateWaitCv.notify_all();

    if (m_thread.joinable())
        m_thread.join();
}

void MapManager::Update()
{
    using Clock = std::chrono::steady_clock;

    constexpr auto updateInterval = std::chrono::milliseconds(50);

      // 첫 업데이트도 약 0.05초로 계산되도록 설정
    auto previousTime = Clock::now() - updateInterval;

    while (m_running.load(std::memory_order_acquire))
    {
        const auto currentTime = Clock::now();

        float deltaTime = std::chrono::duration<float>(currentTime - previousTime).count();

        previousTime = currentTime;

        // 디버깅 중단 등으로 dt가 너무 커지는 것 방지
        deltaTime = std::clamp(deltaTime, 0.0f, 0.25f);

        for (auto iter = m_maps.begin(); iter != m_maps.end(); ++iter)
        {
            if (!m_running.load(std::memory_order_acquire))
                break;

            if (iter->second != nullptr)
            {
                MapInstance *map = iter->second;

                auto task = std::make_unique<MapUpdateTask>(map, deltaTime);
                m_server->GetThreadPool()->Submit(std::move(task));
            }
        }
        RemoveMap();
        std::unique_lock<std::mutex> lock(m_updateWaitMutex);
        m_updateWaitCv.wait_for(lock,updateInterval,[this] { return !m_running.load(std::memory_order_acquire); });
    }

    K_LOG_DEBUG("MapManager Update Stop");

}

MapInstance *MapManager::GetOrCreate(int mapId)
{
    {
        std::lock_guard<std::mutex> lock(m_mapsMutex);

        auto it = m_maps.find(mapId);
        if (it != m_maps.end())
        {
            K_LOG_TRACE( "이미 생성되어 있는 맵입니다. 맵의 정보를 반환합니다.");
            return it->second;
        }
            
    }
    
    MapInitData mapData;

    auto itInit = m_maps_initData.find(mapId);
    if (itInit != m_maps_initData.end())
    {
        mapData = itInit->second;
    }
    else
    {
        if (!LoadJsonFile(mapId, mapData))
            return nullptr;
    }

    MapInstance *newMap = new MapInstance();

    newMap->SetCombatService(m_server->GetCombatService());

    if (newMap->Init(mapData) != 1)
    {
        delete newMap;
        return nullptr;
    }

    // 맵 생성 후 삭제 예약을 걸어둔다. 맵에 플레이어가 없는 경우에 일정 시간이 지나면 맵을 삭제할 수 있도록 설정
    newMap->SetDestroyCallback([this](int id)
    { 
        std::lock_guard<std::mutex> lock(m_destroyQueueMutex);
        m_destroyQueue.push(id); 
    });


    {
        std::lock_guard<std::mutex> lock(m_mapsMutex);

        auto it = m_maps.find(mapId);
        if (it != m_maps.end())
        {
            delete newMap;
            return it->second;
        }

        m_maps[mapId] = newMap;
    }
    return newMap;
}

bool MapManager::PreLoadAll()
{
    for (const auto &entry : fs::directory_iterator(MAP_PATH))
    {
        if (!entry.is_regular_file())
            continue;
        if (entry.path().extension() != ".json")
            continue;

        /*
            // entry.path() -> 파일 경로
            // entry.path().filename() -> 파일명
            // entry.path().stem() -> 확장자 제거한 파일명(예: "1001")

        */
        int map_id = std::stoi(entry.path().stem().string());

        auto it = m_maps_initData.find(map_id);
        if (it != m_maps_initData.end())
            continue;

        MapInitData mapData;

        bool is_Load = LoadJsonFile(map_id, mapData);
        if (!is_Load)
            return false;

        m_maps_initData.emplace(map_id, std::move(mapData));
    }
    K_LOG_TRACE( "Map PreLoadAll Success");
    return true;
}

bool MapManager::LoadJsonFile(int mapId, MapInitData &mapData)
{

    std::string path = MAP_PATH + std::to_string(mapId) + ".json";
    std::ifstream file(path);

    if (!file.is_open())
    {
        K_LOG_ERROR( "FAILED OPEN [%s] FILE", path.c_str());
        return false;
    }
    
    // JSON 파일 파싱
    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (const nlohmann::json::parse_error &e)
    {
        // JSON 문법 깨짐/파싱 실패
        return false;
    }

    if (j.is_null())
        return false;

    mapData.name = j.at("name").get<std::string>();
    mapData.mapID = j.at("mapId").get<u_int32_t>();
    // Json 파일에서 몬스터 데이터 읽어오기
    LoadMonster(j, mapData.MonstersData);
    LoadPortal(j, mapData.portals, mapData.mapID);
    return true;
}

void MapManager::LoadMonster(nlohmann::json &j, std::vector<MonsterSpawnData>& MonstersData)
{
    const auto& arr = j.at("monsters");
    MonstersData.clear();
    MonstersData.reserve(arr.size());

    for (const auto& m : arr)
    {
        MonsterSpawnData data;
        data.monsterId = m.at("monsterId").get<int>();   // 키 맞춰라
        data.respawnDelay = j.at("respawnDelay").get<u_int32_t>();;
        data.spawnPos.xPos = m.at("xPos").get<float>();     
        data.spawnPos.yPos = m.at("yPos").get<float>();
        data.ItemId = m.at("group").get<int>(); 

        MonstersData.push_back(std::move(data));
    }
}

void MapManager::LoadPortal(nlohmann::json& j, std::vector<PortalData>& portals, u_int32_t mapId)
{

    if (!j.contains("portals"))
        return;

    const auto& portal = j.at("portals");

    portals.clear();
    portals.reserve(portal.size());

    for (const auto& portalJson : portal)
    {
        PortalData data;
        data.portalId = portalJson.at("id").get<std::string>();   // 키 맞춰라
        data.sourceMapId = mapId;
        data.destinationMapId = portalJson.at("destinationMapId").get<int>();     
        const auto& position = portalJson.at("position");
        data.position.xPos = position.at("x").get<float>();
        data.position.yPos = position.at("y").get<float>();
        const auto& spawnPosition = portalJson.at("spawnPosition");
        data.spawnPosition.xPos = spawnPosition.at("x").get<float>();
        data.spawnPosition.yPos = spawnPosition.at("y").get<float>();
        data.interactionRange =portalJson.value("interactionRange",100.0f);
        portals.push_back(std::move(data));
    }
}

void MapManager::RemoveMap()
{
    // m_maps, m_destroyQueue 접근은 mutex로 보호했고, 
    // MapInstance 수명 문제는 shared_ptr 기반 관리 또는 삭제 지연 큐로 개선할 수 있도록 식별했다.
    std::scoped_lock(m_destroyQueueMutex, m_mapsMutex);
    while (!m_destroyQueue.empty())
    {
        int mapId = m_destroyQueue.front();
        m_destroyQueue.pop();

        auto it = m_maps.find(mapId);
        if (it != m_maps.end())
        {
            K_LOG_TRACE( "Map Delete [%d]", mapId);
            delete it->second;
            it->second = nullptr;
            m_maps.erase(it);
        }
    }
}