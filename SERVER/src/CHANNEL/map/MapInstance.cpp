#include "common.h"
#include "MapInstance.h"
#include "Monster.h"
#include "CommonEnum.h"
#include "IProjectile.h"
#include "timeUtility.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include "MonsterPacketSender.h"
#include "PlayerPacketSender.h"
#include "ItemPacketSender.h"
#include "StatInfoPacket.h"


#define MAPDELETELIMIT 5
namespace
{
	struct DeadMonsterInfo
	{
	    int exp = 0;
	    std::string commonGroupId;
	    std::string uniqueGroupId;
	    Vec2 dropPos{};
	    Player* owner = nullptr;
	};
	
	struct ContactDamageEvent
	{
    	Player* player = nullptr;
    	PlayerHitResult result;
	};
}


MapInstance::MapInstance() : m_playerCount(0), m_limit(std::chrono::minutes{MAPDELETELIMIT}), m_combatService(nullptr)
{
	m_monsterManager = MonsterManager::GetInstance();
	m_dropManager = DropManager::GetInstance();
	m_contactCheckRadiusSq = 1000.0f; //임시 몸박 최소 데미지 거리
}

MapInstance::~MapInstance()
{

}

int MapInstance::Init(const MapInitData& data)
{
    this->m_mapID = data.mapID;
	// 여기서 Map Json 파일에서 읽어온 몬스터 정보 저장
    this->m_monsterSpawnList = data.MonstersData;

	for(const PortalData& portal : data.portals)
	{
		m_portals.emplace(portal.portalId, portal);
	}

#if 0 //guno22_TEST
	{
		//"100000000”
		std::vector<MonsterSpawnData>& list = this->m_monsterSpawnList;
		K_LOG_DEBUG( "gunoo22_TEST size[%d]", list.size());
		for (auto& m : list)
		{
			K_LOG_DEBUG( "gunoo22_TEST m.monsterId[%d]", m.monsterId);
		}
	}
#endif
	InitSpawnMonster();
    return 1;
}

int MapInstance::Update()
{
	//K_LOG_TRACE( "MapInstance Pointer[%p]", this);
	if(!HasPlayer())
	{
		RemoveMap();
	}
	else
	{
		SpawnMonster();
		UpdateMonster();
		SendMapInfo();
		//m_projectileManager.Update(); //투사체 업데이트
		//ProcessRangedDamage(NowMs()); //원거리 공격 판정 및 데미지 처리
		ProcessContactDamage(NowMs()); //플레이어-몬스터 접촉 판정 및 데미지 처리
	}

    return 1;
}
//맵내에 있는 모든사용자에게 Update시 보내주는 정보
void MapInstance::SendMapInfo()
{
	std::vector<Player*> players;
	// 락 안에서 SnedMonster까지 보낼려면 너무 오랜시간 Lock을 잡고 있어 별로라서
	// 락 안에서 PlayerList를 복사하고 복사본을 가지고 SendMonsterMove 하는 것이 좋다.
	{
        std::lock_guard<std::mutex> lock(m_playerMutex);

        for (auto& [id, player] : m_playerList)
        {
            if (player != nullptr)
                players.push_back(player);
        }
    }

	for (Player* player : players)
    {
        SendMonsterMove(player);
    }
}

int MapInstance::InitSpawnMonster()
{
	int instanceId = 1;
    m_monsterList.reserve(m_monsterSpawnList.size());
	
    for(m_monsterSpawnListIter = m_monsterSpawnList.begin(); m_monsterSpawnListIter < m_monsterSpawnList.end(); ++m_monsterSpawnListIter)
    {
        Monster monster;
       
		
		if(!m_monsterManager->EnsureLoaded(m_monsterSpawnListIter->monsterId))
		{
			K_LOG_ERROR( "FAILED OPEN [%s] FILE", m_monsterSpawnListIter->monsterId);
			return -1;
		}
			
		
		auto monsterTemplate = m_monsterManager->GetMonsterData(m_monsterSpawnListIter->monsterId);
		
		// 맵에 스폰된 몬스터들 끼리 구별하기 위한 값
		m_monsterSpawnListIter->instanceId = instanceId;
		instanceId++;

		
		if(monsterTemplate.has_value())
		{
			(*monsterTemplate).mapId = m_mapID;
			(*monsterTemplate).mapInstance = this;
			monster.Init(*monsterTemplate, *m_monsterSpawnListIter);
		}else {
			K_LOG_ERROR( "MonsterTemplate Get Failed Monster_Id[%d] FILE", m_monsterSpawnListIter->monsterId);
			return -1;
		}

        m_monsterList.push_back(monster);
		
    }


    return 1;
}   

int MapInstance::UpdateMonster()
{
	//K_LOG_ERROR( "몬스터 업데이트 시작");
	std::lock_guard<std::mutex> lock(m_monsterMutex);
	for(auto& monster : m_monsterList) 
	{
		if(monster.IsAlive())
		{
			monster.Update();
		}
	}

	return 1;
}

int MapInstance::SpawnMonster()
{
	auto now = std::chrono::steady_clock::now();

	std::vector<MonsterRespawnInfo> respawnList;
	
	{
		std::lock_guard<std::mutex> lock(m_monsterMutex);
		//K_LOG_TRACE( "몬스터 리스폰 시작");
		for(auto& monster : m_monsterList) 
		{
			if(monster.IsAlive()) continue;
			
			if(monster.CheckRespawnTime(now)) 
			{
				monster.Reset();
				MonsterRespawnInfo monsterRespawnInfo{};

				monsterRespawnInfo.instanceId = monster.GetInstanceId();
				monsterRespawnInfo.monsterId = monster.GetId();
				monsterRespawnInfo.xPos = monster.GetPos().xPos;
				monsterRespawnInfo.yPos = monster.GetPos().yPos;
				monsterRespawnInfo.dirX = static_cast<int>(monster.GetDir().xPos);
				monsterRespawnInfo.currentHp = monster.GetCurrentHP();
				monsterRespawnInfo.MaxHp = monster.GetMaxHP();
				monsterRespawnInfo.state = static_cast<int>(monster.GetState()); 

				respawnList.push_back(monsterRespawnInfo);
			}
		}
	}

	std::unordered_map<int, Player*> playerSnapshot;

	{
	    std::lock_guard<std::mutex> lock(m_playerMutex);
	    playerSnapshot = m_playerList;
	}

	if(!respawnList.empty())
	{
		MonsterPacketSender::SendMonsterRespawn(playerSnapshot, respawnList);
	}
	
    return 1;
}

void MapInstance::OnEnter(int PlayerID, Player* player)
{
	//int playerCount = 0;
	{
		std::lock_guard<std::mutex> lock(m_playerMutex);
		auto it = m_playerList.find(PlayerID);

		if(it != m_playerList.end()) return;
	

		m_playerList[PlayerID] = player;
		m_playerCount = static_cast<int>(m_playerList.size());

		if (m_playerCount > 0)
        {
            m_has_player = true;
            m_destroyRequested = false;
            m_emptyTime = {};
        }
		//playerCount = m_playerCount;
	}
	//K_LOG_DEBUG( "PlayerID(%d)", PlayerID);
	//K_LOG_DEBUG( "m_playerCount(%d)", playerCount);
	// 들어온 플레이어 한테 몬스터 정보 전달
	SendMonsterSnapshot(player);
}

void MapInstance::OnLeave(int PlayerID)
{
    int playerCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_playerMutex);

        auto it = m_playerList.find(PlayerID);
        if (it == m_playerList.end())
            return;

        m_playerList.erase(it);

        m_playerCount = static_cast<int>(m_playerList.size());

        if (m_playerCount == 0)
        {
            m_emptyTime = std::chrono::steady_clock::now();
            m_has_player = false;
            m_destroyRequested = false;
        }

        playerCount = m_playerCount;
    }

	K_LOG_DEBUG( "PlayerID(%d)", PlayerID);
    K_LOG_DEBUG( "m_playerCount(%d)", playerCount);
}

void MapInstance::GiveExp(int playerID, float exp)
{
	(void)playerID;
	(void)exp;
}

void MapInstance::HandleMove(Player* sender, Vec2 pos, float speed, int dir)
{
	if(!sender) return;

	//K_LOG_TRACE( "플레이어 ID [%d]", sender->GetId());
	std::unordered_map<int, Player*> playerSnapshot;
	{
		std::lock_guard<std::mutex> lock(m_playerMutex);
		auto it = m_playerList.find(sender->GetId());

		if(it == m_playerList.end())
		{
			K_LOG_ERROR( "[%d]해당 맵에 존재하지 않은 플레이어 입니다.", m_mapID);
			return;
		}
		sender->SetPos(pos);
		playerSnapshot = m_playerList;
	}

	

	PlayerPacketSender::SendPlayersMove(sender, pos, speed, dir, playerSnapshot);
}

void MapInstance::SendMonsterSnapshot(Player* player)
{
     if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr");
        return;
    }

    std::vector<MonsterSnapshotInfo> aliveMonsters;
	// Monster*를 lock 밖으로 넘기면, 패킷 생성 시점에 다른 스레드가 Monster 상태를 변경할 수 있다.
	// 따라서 lock 안에서 전송에 필요한 값만 snapshot으로 복사한 뒤, lock 밖에서 패킷을 전송한다.
    {
        std::lock_guard<std::mutex> lock(m_monsterMutex);

        aliveMonsters.reserve(m_monsterList.size());

        for (auto& monster : m_monsterList)
        {
            if (!monster.IsAlive())
                continue;

            MonsterSnapshotInfo info;
            info.instanceId = monster.GetInstanceId();
            info.monsterId = monster.GetId();
            info.state = static_cast<int>(monster.GetState());
            info.dirX = static_cast<int>(monster.GetDir().xPos);
            info.xPos = monster.GetPos().xPos;
            info.yPos = monster.GetPos().yPos;
            info.currentHp = monster.GetCurrentHP();
            info.maxHp = monster.GetMaxHP();
			info.moveSpeed = monster.GetMoveSpeed();

            aliveMonsters.push_back(info);
        }
    }

    MonsterPacketSender::SendMonsterSnapShot(player, aliveMonsters);
}

 void MapInstance::SendMonsterMove(Player* player)
 {
	  if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr");
        return;
    }

    std::vector<MonsterMoveInfo> aliveMonsters;
	{
		std::lock_guard<std::mutex> lock(m_monsterMutex);
    	aliveMonsters.reserve(m_monsterList.size());
    	for (auto& monster : m_monsterList)
    	{
        	if (!monster.IsAlive())
        	    continue;

        	monster.SetState(MonsterState::E_Move);

        	MonsterMoveInfo info;
        	info.instanceId = monster.GetInstanceId();
        	info.state = static_cast<int>(monster.GetState());
        	info.dirX = static_cast<int>(monster.GetDir().xPos);
        	info.xPos = monster.GetPos().xPos;
        	info.yPos = monster.GetPos().yPos;
        	info.currentHp = monster.GetCurrentHP();
        	info.maxHp = monster.GetMaxHP();

        	aliveMonsters.push_back(info);
    	}
	}
	MonsterPacketSender::SendMonsterMove(player, aliveMonsters);
 }

// 맵이 사라지는 경우 호출
void MapInstance::RemoveMap()
{
	auto now = std::chrono::steady_clock::now();

    std::function<void(int)> destroyCallback;
    int mapId = 0;

    {
        std::lock_guard<std::mutex> lock(m_playerMutex);

        if (m_destroyRequested)
            return;

        if ((now - m_emptyTime) < m_limit)
            return;

        m_destroyRequested = true;
        destroyCallback = m_onDestroyReq;
        mapId = m_mapID;
    }

    K_LOG_TRACE( "[%d]맵 삭제", mapId);

    if (destroyCallback)
        destroyCallback(mapId);
}

// 나중에 PacketSender로 옮겨야함
void MapInstance::ResolveSkillHit(Player* Attacker, SkillDef& skillDef, std::vector<std::pair<Monster*, int>>& hits)
{
	 if (Attacker == nullptr)
        return;

    std::vector<MonsterHitResult> results;
    std::vector<DeadMonsterInfo> deadMonsters;
    std::unordered_map<int, Player*> playerSnapshot;

    results.reserve(hits.size());

    {
        std::lock_guard<std::mutex> lock(m_monsterMutex);

        for (auto& [monster, dmg] : hits)
        {
            if (monster == nullptr)
                continue;

            bool isDead = monster->OnDamaged(Attacker, dmg);

            if (isDead)
            {
                DeadMonsterInfo deadInfo;
                deadInfo.exp = monster->GetExp();
                deadInfo.commonGroupId = monster->GetCommonItemGroupID();
                deadInfo.uniqueGroupId = monster->GetUniqueItemGroupID();
                deadInfo.dropPos = monster->GetPos();
                deadInfo.owner = monster->GetLastAttacker();

                deadMonsters.push_back(deadInfo);
            }

            MonsterHitResult hitResult;
            hitResult.monster_instance_id = monster->GetInstanceId();
            hitResult.damage = dmg;
            hitResult.cur_hp = monster->GetCurrentHP();
            hitResult.max_hp = monster->GetMaxHP();
            hitResult.dead = isDead;

            results.push_back(hitResult);
        }
    }

    for (auto& dead : deadMonsters)
    {
        const ExpResult expResult = Attacker->AddExp(dead.exp);
        PlayerPacketSender::SendExpGain(Attacker, expResult);

        if (expResult.levelUp)
            PlayerPacketSender::SendPlayerStat(Attacker);

        std::vector<DropResult> dropItems =
            m_dropManager->SetDropItem(dead.commonGroupId, dead.uniqueGroupId);

        SpawnDropItem(dead.dropPos, dead.owner, dropItems);
    }

    {
        std::lock_guard<std::mutex> lock(m_playerMutex);
        playerSnapshot = m_playerList;
    }

    MonsterPacketSender::SendMonsterOnDamaged(Attacker, skillDef.skill_id, results, playerSnapshot);
}


void MapInstance::ProcessContactDamage(int64_t nowMs)
{
	std::vector<ContactDamageEvent> events;
    std::unordered_map<int, Player*> playerSnapshot;

	//K_LOG_DEBUG( "START");
	{
		std::scoped_lock lock(m_playerMutex, m_monsterMutex);

        playerSnapshot = m_playerList;
	   	for(auto p : m_playerList)
	   	{
			Player* player = p.second;

			// 플레이어가 죽었다면 스킵
			//K_LOG_DEBUG( "player->IsAlive()");
			if(!player || !player->IsAlive()) continue;

			// 플레이어가 이미 피격 당했고 무적 상태라면 스킵
			//K_LOG_DEBUG( "CanTakeAnyContactDamage");
			if(!player->CanTakeAnyContactDamage(nowMs)) continue;

			const Vec2 player_pos = player->GetPos();

			for(Monster& monster : m_monsterList)
			{
				// 몬스터가 죽은 상태라면 스킵
				//K_LOG_DEBUG( "monster.IsAlive");
				if(!monster.IsAlive()) continue;

			const Vec2 monster_pos = monster.GetPos();
			// 플레이어와 몬스터의 거리가 일정 이상이라면 스킵
			//K_LOG_DEBUG( "m_contactCheckRadiusSq [%f]", m_contactCheckRadiusSq);
			//K_LOG_DEBUG( "Distancesquare(player_pos, monster_pos) [%f]", Distancesquare(player_pos, monster_pos));
			if(Distancesquare(player_pos, monster_pos) > m_contactCheckRadiusSq) continue;
			
			//K_LOG_DEBUG( "Intersects");
			 // 정밀 충돌(AABB/원형)
            if (!Collision::Intersects(player_pos, player->GetCollider(), monster_pos, monster.GetCollider())) continue;

			//K_LOG_DEBUG( "ApplyContactDamage");
			int dmg = m_combatService->ApplyContactDamage(player, monster);
			
			//K_LOG_DEBUG( "OnDamaged");
			player->OnDamaged(dmg,nowMs);
			PlayerHitResult result;
			result.damage = dmg;
			SetPlayerHitResult(player, monster.GetInstanceId(), result);
			
			events.push_back({ player, result });
			//K_LOG_DEBUG( "SendPlayerOnDamaged");
			}
		}
	//K_LOG_DEBUG( "END\n\n");
	}

	for (const auto& event : events)
    {
        PlayerPacketSender::SendPlayerOnDamaged(event.player, event.result, playerSnapshot);
    }
}
/*gunoo22 260223 원거리 공격 처리*/
void MapInstance::ProcessRangedDamage(int64_t nowMs)
{
	{
		std::lock_guard<std::mutex> lock(m_playerMutex);
   		for(auto p : m_playerList)
   		{
				Player* player = p.second;
		
				// 플레이어가 죽었다면 스킵
				if(!player || !player->IsAlive()) continue;
		
				// 플레이어가 이미 피격 당했고 무적 상태라면 스킵
				if(!player->CanTakeAnyContactDamage(nowMs)) continue;
		
				const Vec2 player_pos = player->GetPos();
		
				K_LOG_TRACE( "\n\nPLAYER POS [%f, %f]", player_pos.xPos, player_pos.yPos);
				for (const auto& p : m_projectileManager.GetProjectiles())
				{
					const Vec2& projectile_pos = p->GetPos();
				
					K_LOG_DEBUG( "PROJECTILE POS [%f, %f]", projectile_pos.xPos, projectile_pos.yPos);
				
					//플레이어와 투사체 거리가 일정거리 이상이라면 스킵
					if (Distancesquare(player_pos, projectile_pos) > m_contactCheckRadiusSq) continue;
				
					 // 정밀 충돌(AABB/원형)
					if (!Collision::Intersects(player_pos, player->GetCollider(), projectile_pos, p->GetCollider())) continue;
				
					//플레이어 온데미지
					player->OnDamaged(p->GetDamage(), nowMs);
				}
   		}
	}
}

void MapInstance::SetPlayerHitResult(Player* player, int monster_instanceId, PlayerHitResult& result)
{
	result.attacker_instance_id = monster_instanceId;
	result.player_id = player->GetId();
	result.cur_hp = player->GetCurHP();
	result.max_hp = player->GetMaxHP();
	result.state = player->GetState();
}

bool MapInstance::PickupDropItem(Player *player, int dropItemId, std::vector<AddItemResult>& addItemResults)
{
	if (player == nullptr)
	{
		K_LOG_ERROR( "player is nullptr.\n");
		return false;
	}
       DropItems dropItem;
	{
		std::lock_guard<std::mutex> lock(m_dropItemMutex);
    	auto it = m_dropItems.find(dropItemId);
    	if (it == m_dropItems.end())
		{
			K_LOG_ERROR( "dropItemId [%d] is nullptr.\n", dropItemId);
			return false;
		}
	
    	 dropItem = it->second;
	}

    if (!CanPickupByDistance(player->GetPos(), dropItem.pos))
	{
		K_LOG_TRACE( "CanPickupByDistance.\n");
		return false;
	}
        

    if (dropItem.owner != nullptr &&dropItem.owner->GetId() != player->GetId())
	{
			K_LOG_TRACE( "아이템 소유권이 없습니다.\n");
			return false;
	}

	{
        std::lock_guard<std::mutex> lock(m_dropItemMutex);

        auto it = m_dropItems.find(dropItemId);
        if (it == m_dropItems.end())
            return false;

        m_dropItems.erase(it);
    }
        

    InventoryManager* inven = player->GetInventoryManager();
    if (inven == nullptr)
	{
		std::lock_guard<std::mutex> lock(m_dropItemMutex);
		K_LOG_ERROR( "inven is nullptr.\n");
    	m_dropItems[dropItem.dropId] = dropItem;
		return false;
	}
    K_LOG_TRACE( "dropItem.itemId [%d].\n", dropItem.itemId);
	K_LOG_TRACE( "dropItem.dropId [%d].\n", dropItem.dropId);
    if (!inven->AddItem(dropItem.itemId, dropItem.count, addItemResults))
	{
		std::lock_guard<std::mutex> lock(m_dropItemMutex);
    	m_dropItems[dropItem.dropId] = dropItem;
		return false;
	}
    	
	std::unordered_map<int, Player*> playerSnapshot;
    {
        std::lock_guard<std::mutex> lock(m_playerMutex);
        playerSnapshot = m_playerList;
    }
	
    ItemPacketSender::SendRemoveDropItem({dropItemId},playerSnapshot);
    return true;
}

bool MapInstance::CanPickupByDistance(Vec2 playerPos, Vec2 ItemPos)
{
   constexpr float PICKUP_DISTANCE = 80.0f;

    float dx = ItemPos.xPos - playerPos.xPos;
    float dy = ItemPos.yPos - playerPos.yPos;

    float distanceSq = dx * dx + dy * dy;

    return distanceSq <= PICKUP_DISTANCE * PICKUP_DISTANCE;
}

bool MapInstance::SpawnDropItem(const Vec2& dropPos, Player* owner, const std::vector<DropResult>& dropItems)
{
	std::vector<DropSpawnInfo> spawnInfos;
    std::unordered_map<int, Player*> playerSnapshot;
	const auto nowMs = NowMs();
    {
        std::lock_guard<std::mutex> lock(m_dropItemMutex);
		
        for (const auto& drop : dropItems)
        {
            DropItems item;
            item.count = drop.count;
            item.itemId = drop.itemId;
            item.type = drop.type;
            item.dropId = m_dropId++;
            item.pos = dropPos;
            item.owner = owner;

			
            item.ownerExpireTimeMs = nowMs + 60000;
            item.expireTimeMs = nowMs + 120000;

            m_dropItems[item.dropId] = item;

            DropSpawnInfo info;
            info.dropId = item.dropId;
            info.count = item.count;
            info.itemId = item.itemId;
            info.xPos = item.pos.xPos;
            info.yPos = item.pos.yPos;

            spawnInfos.push_back(info);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_playerMutex);
        playerSnapshot = m_playerList;
    }
	if (spawnInfos.empty())
    	return true;
    ItemPacketSender::SendSpawnItem(spawnInfos, playerSnapshot);
    return true;
}

void MapInstance::CheckDropItem()
{
	int64_t nowMs = NowMs();
	std::vector<int> removeItems;

	{
		std::lock_guard<std::mutex> lock(m_dropItemMutex);
		for (auto it = m_dropItems.begin(); it != m_dropItems.end(); )
		{
    		auto& item = it->second;
		
    		if (item.owner != nullptr && item.ownerExpireTimeMs <= nowMs)
    		{
    		    item.owner = nullptr; // 누구나 먹을 수 있게
    		}
		
    		if (item.expireTimeMs <= nowMs)
    		{
				//삭제 아이템에 대한 정보 전달
				removeItems.push_back(it->second.dropId);
    		    it = m_dropItems.erase(it);
    		}
    		else
    		{
    		    ++it;
    		}
		}
	}
	
	if (removeItems.empty())
    	return;

	std::unordered_map<int, Player*> playerSnapshot;
	{
		std::lock_guard<std::mutex> lock(m_playerMutex);
        playerSnapshot = m_playerList;
	}
	ItemPacketSender::SendRemoveDropItem(removeItems, playerSnapshot);
}

std::optional<PortalData> MapInstance::FindPortal(const std::string &portalId) const
{
	auto iter = m_portals.find(portalId);

	if(iter == m_portals.end())
		return std::nullopt;

	return iter->second;
}

bool MapInstance::HasPlayer()
{
	std::lock_guard<std::mutex> lock(m_playerMutex);
	return m_has_player;
}
