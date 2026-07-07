#include "common.h"
#include "MapManager.h"
#include "PlayerManager.h"
#include "MapUpdateTask.h"
#include "ChannelServer.h"

#define MAP_PATH "../src/CHANNEL/data/Maps/"
#define UPDATE_INTERVAL 2000 //2초
namespace fs = std::filesystem;

MapManager::MapManager(ChannelServer *server) : m_server(server)
{
}

MapManager::~MapManager()
{
}

bool MapManager::Init()
{
    // 서버 구동 시 Map 데이터 전부 읽어서 미리 저장
    if(!PreLoadAll()) return false;
    return true;
}

void MapManager::Start()
{
    m_running = true;
    m_thread = std::thread(&MapManager::Update, this);
    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] MapManager Update Start", __FILE__, __LINE__);
}

void MapManager::Stop()
{
    m_running = false;
    if (m_thread.joinable())
        m_thread.join();
}

void MapManager::Update()
{
    while (m_running)
    {
        for (auto iter = m_maps.begin(); iter != m_maps.end(); ++iter)
        {
            if (iter->second != nullptr)
            {
                MapInstance *map = iter->second;

                auto task = std::make_unique<MapUpdateTask>(map);
                m_server->GetThreadPool()->Submit(std::move(task));
            }
        }
        RemoveMap();
        std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_INTERVAL)); // 간격
    }
}

MapInstance *MapManager::GetOrCreate(int mapId)
{
    {
        std::lock_guard<std::mutex> lock(m_mapsMutex);

        auto it = m_maps.find(mapId);
        if (it != m_maps.end())
        {
            K_slog_trace(K_SLOG_TRACE, "[%s][%d] 이미 생성되어 있는 맵입니다. 맵의 정보를 반환합니다.", __FUNCTION__, __LINE__);
            return it->second;
        }
            
    }
    
    MapInitData mapData;

    auto itInit = m_maps_initData.find(mapId);
    if (itInit != m_maps_initData.end())
    {
        K_slog_trace(K_SLOG_TRACE, "[%s][%d]gunoo22_TEST", __FUNCTION__, __LINE__);
        mapData = itInit->second;
    }
    else
    {
        K_slog_trace(K_SLOG_TRACE, "[%s][%d]gunoo22_TEST", __FUNCTION__, __LINE__);
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
    K_slog_trace(K_SLOG_TRACE, "[%s][%d] Map PreLoadAll Success", __FUNCTION__, __LINE__);
    return true;
}

bool MapManager::LoadJsonFile(int mapId, MapInitData &mapData)
{

    std::string path = MAP_PATH + std::to_string(mapId) + ".json";
    std::ifstream file(path);

    if (!file.is_open())
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] FAILED OPEN [%s] FILE", __FUNCTION__, __LINE__, path.c_str());
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
            K_slog_trace(K_SLOG_TRACE, "[%s][%d] Map Delete [%d]", __FUNCTION__, __LINE__, mapId);
            delete it->second;
            it->second = nullptr;
            m_maps.erase(it);
        }
    }
}