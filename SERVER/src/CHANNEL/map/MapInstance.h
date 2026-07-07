#pragma once
#include "common.h"
#include "PlayerManager.h"
#include "CommonEnum.h"
#include "Skill_Info.h"
#include "Monster.h"
#include "MonsterManager.h"
#include "Item.h"
#include "Player.h"
#include "ChannelSession.h"
#include "CombatService.h"
#include "DropManager.h"
#include "Skill_Info.h"
#include "Inventory.h"
#include "DropInfos.h"
#include "ProjectileManager.h"

#include <nlohmann/json.hpp>
#include <functional>
#include <mutex>

class CombatService;

class MapInstance 
{
public:
    MapInstance();
    ~MapInstance();
    
    int Init(const MapInitData& data);
    int InitSpawnMonster();
    int SetMonster(MonsterType monsterType);

    void SetCombatService(CombatService* combatservice){m_combatService = combatservice;};
    //int LoadMonsters(int monster_id);
    int Update();
    int UpdateMonster();
    int SpawnMonster();

    void RemoveMap();
	
	int checkPlayer(int PlayerID);
	
	// 플레이어 들어왔을 때 처리 함수
	void OnEnter(int PlayerID, Player* player);

	// 플레이어 나갔을 때 처리 함수
	void OnLeave(int PlayerID);

    void GiveExp(int platerID, float exp);
    void HandleMove(Player* sender, Vec2 pos, float speed);
    void HandleMove(Player* sender, Vec2 pos, float speed, int dir);
    void ResolveSkillHit(Player* Attacker, SkillDef& skillDef, std::vector<std::pair<Monster*, int>>& hits);
    void SetPlayerHitResult(Player* player, int monster_instanceId, PlayerHitResult& result);
    bool PickupDropItem(Player* player, int dropItemId, std::vector<AddItemResult>& addItemResults);
    bool CanPickupByDistance(Vec2 playerPos, Vec2 ItemPos);
    bool SpawnDropItem(const Vec2& dropPos, Player* owner, const std::vector<DropResult>& dropItems);
    void CheckDropItem();
    
private:
    void BroadcastDropSpawn(std::vector<DropSpawnInfo> spawnedInfos);
    void BroadcastRemoveItem(std::vector<int> removeItems);
    void SendMapInfo();
    void SendMonsterSnapshot(Player* Enter_player);  
    void SendMonsterMove(Player* player);

    // 몬스터와 플레이어의 접촉 시 
    void ProcessContactDamage(int64_t nowMs);
    void ProcessRangedDamage(int64_t nowMs);
public:

    // int 매개변수를 받는 콜백 함수 이름 지정
    using DestroyReqFn = std::function<void(int m_mapID)>;

    // 나중에 호출할 콜백 함수를 변수에 저장하는 함수
    void SetDestroyCallback(DestroyReqFn cb) {m_onDestroyReq = std::move(cb);}

    uint16_t GetMapId() {return m_mapID;}

    std::vector<Monster>& GetMonsterList(){ return m_monsterList;};
    ProjectileManager& GetProjectileManager() { return m_projectileManager; }

    bool HasPlayer();

    std::unordered_map<int, Player*>& GetPlayerList(){return m_playerList;}
private:
   	// 플레이어가 맵에 있는지 없는지 판단 변수
    bool m_has_player;
    bool m_destroyRequested;

    // 플레이어-몬스터 접촉 판정용 거리 임계값(반지름^2). 거리^2와 비교한다.
    float m_contactCheckRadiusSq;
	// 플레이어 수 
	uint16_t m_playerCount;
    uint16_t m_mapID;
    int m_dropId;

    std::unordered_map<int, Player*> m_playerList;
    std::unordered_map<int, DropItems> m_dropItems;
   
    std::vector<MonsterSpawnData> m_monsterSpawnList;
    std::vector<MonsterSpawnData>::iterator m_monsterSpawnListIter;

    std::vector<Monster> m_monsterList;
    std::vector<Monster>::iterator m_monsterListIter;

    std::vector<Item> m_itemList;
    std::vector<DropSpawnInfo> m_spawnInfos;
	// Map에 플레이어가 없을 때 딱 시간 찍는 변수
	std::chrono::steady_clock::time_point m_emptyTime;
    // Map 사라지는 제한 시간
    std::chrono::minutes m_limit;

    // 맵이 사라질 때 반환하는 예약 콜백 함수 
    DestroyReqFn m_onDestroyReq;
    ProjectileManager m_projectileManager;

    // 동기화
    std::mutex m_playerMutex;
    std::mutex m_dropItemMutex;
    std::mutex m_monsterMutex;
private:
    MonsterManager* m_monsterManager;
    CombatService* m_combatService;
    DropManager* m_dropManager;
};