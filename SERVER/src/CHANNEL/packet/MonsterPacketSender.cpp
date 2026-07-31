#include "MonsterPacketSender.h"
#include "ChannelSession.h"
#include "K_slog.h"


void MonsterPacketSender::SendMonsterSnapShot(Player* player, const std::vector<MonsterSnapshotInfo>& monsters)
{
    if (player == nullptr)
        return;

    auto session = player->GetSession();
    if (session == nullptr)
        return;

    std::vector<std::string> payload;
    payload.reserve(1 + monsters.size() * 8);

    payload.push_back(std::to_string(monsters.size()));

    for (const auto& monster : monsters)
    {
        payload.push_back(std::to_string(monster.monsterId));          // monsterTemplateId
        payload.push_back(std::to_string(monster.instanceId));  // monsterObjectId
        payload.push_back(std::to_string(monster.xPos));
        payload.push_back(std::to_string(monster.yPos));
        payload.push_back(std::to_string(monster.dirX));
        payload.push_back(std::to_string(monster.moveSpeed));
        payload.push_back(std::to_string(monster.currentHp));
        payload.push_back(std::to_string(monster.maxHp));
        payload.push_back(std::to_string(monster.state));
    }
    session->Send(PKT_MONSTER_SNAPSHOT, payload);
}

void MonsterPacketSender::SendMonsterMove(Player* player, const std::vector<MonsterMoveInfo>& monsters)
{
    if (player == nullptr)
        return;

    auto session = player->GetSession();
    if (session == nullptr)
        return;

    std::vector<std::string> payload;
    payload.reserve(1 + monsters.size() * 7);

    payload.push_back(std::to_string(monsters.size()));

    for (const auto& monster : monsters)
    {
        payload.push_back(std::to_string(monster.instanceId));
        payload.push_back(std::to_string(monster.state));
        payload.push_back(std::to_string(monster.dirX));
        payload.push_back(std::to_string(monster.xPos));
        payload.push_back(std::to_string(monster.yPos));
        payload.push_back(std::to_string(monster.currentHp));
        payload.push_back(std::to_string(monster.maxHp));
    }

    session->Send(PKT_MONSTER_MOVE, payload);
}

void MonsterPacketSender::SendProjectileMove(Player* player, const std::vector<ProjectileSnapshotInfo>& projectiles)
{
    if (player == nullptr)
        return;

    auto session = player->GetSession();
    if (session == nullptr)
        return;

    std::vector<std::string> payload;
    payload.reserve(1 + projectiles.size() * 4);

    payload.push_back(std::to_string(projectiles.size()));

    //gunoo22 260726 투사체부분 클라이언트 패킷수신부분과 맞춰줘야함.
    for (const auto& projectile : projectiles)
    {
        payload.push_back(std::to_string(projectile.instanceId));
        payload.push_back(std::to_string(projectile.projectileTypeId));
        payload.push_back(std::to_string(projectile.ownerMonsterId));
        payload.push_back(std::to_string(projectile.dirX));
        payload.push_back(std::to_string(projectile.dirY));
        payload.push_back(std::to_string(projectile.range));
        payload.push_back(std::to_string(projectile.speed));
        payload.push_back(std::to_string(projectile.xPos));
        payload.push_back(std::to_string(projectile.yPos));

        // K_LOG_TRACE( "projectileTypeId [%d]", projectile.projectileTypeId);
        // K_LOG_TRACE( "ownerMonsterId [%d]", projectile.ownerMonsterId);
        // K_LOG_TRACE( "PROJECTILE POS [%f, %f]", projectile.xPos, projectile.yPos);

        int idx = 0;
        for (const auto &pay: payload)
        {
            K_LOG_TRACE("[%d]%s", ++idx, pay.c_str());
        }
        
    }

    

    session->Send(PKT_PROJECTILE_MOVE, payload);
}


void MonsterPacketSender::SendMonsterOnDamaged(Player* Attacker, int SkillID, std::vector<MonsterHitResult>& result, std::unordered_map<int, Player*>& playerList)
{
    std::vector<std::string> payload;
	payload.reserve(3 + result.size() * 5);

	payload.push_back(std::to_string(result.size()));
	payload.push_back(std::to_string(Attacker->GetId()));
	payload.push_back(std::to_string(SkillID));
	for (const auto& r : result)
	{
    	payload.push_back(std::to_string(r.monster_instance_id));
    	payload.push_back(std::to_string(r.damage));
    	payload.push_back(std::to_string(r.cur_hp));
    	payload.push_back(std::to_string(r.max_hp));
    	payload.push_back(r.dead ? "1" : "0");

            
        K_LOG_TRACE( "damage [%d]", r.damage);
        K_LOG_TRACE( "cur_hp [%d]", r.cur_hp);
        K_LOG_TRACE( "max_hp [%d]", r.max_hp);
	}

    
	for(auto it = playerList.begin(); it != playerList.end(); ++it)
	{
		if (it->second == nullptr)
            return;

        auto session = it->second->GetSession();
        if (session == nullptr)
            continue;

        session->Send(PKT_MONSTER_ONDAMAGED, payload);
    }
}

void MonsterPacketSender::SendMonsterRespawn(std::unordered_map<int, Player *> &playerList, const std::vector<MonsterRespawnInfo>& monsters)
{
    std::vector<std::string> payload;
	
    payload.push_back(std::to_string(monsters.size()));

	for(const auto& monster : monsters)
    {
        payload.push_back(std::to_string(monster.instanceId));
        payload.push_back(std::to_string(monster.monsterId));
        payload.push_back(std::to_string(monster.xPos));
        payload.push_back(std::to_string(monster.yPos));
        payload.push_back(std::to_string(monster.dirX));
        payload.push_back(std::to_string(monster.currentHp));
        payload.push_back(std::to_string(monster.MaxHp));
        payload.push_back(std::to_string(monster.state));
    }
    
	for(auto it = playerList.begin(); it != playerList.end(); ++it)
	{
		auto session = it->second->GetSession();
        if (session == nullptr)
            continue;
		
		session->Send(PKT_MONSTER_RESPAWN, payload);
	}
}
