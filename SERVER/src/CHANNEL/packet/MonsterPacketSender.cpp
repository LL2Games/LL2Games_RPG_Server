#include "MonsterPacketSender.h"
#include "ChannelSession.h"


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

            
        K_slog_trace(K_SLOG_TRACE, "[%s : %s : %d] damage [%d]", __FILE__, __FUNCTION__, __LINE__, r.damage);
        K_slog_trace(K_SLOG_TRACE, "[%s : %s : %d] cur_hp [%d]", __FILE__, __FUNCTION__, __LINE__, r.cur_hp);
        K_slog_trace(K_SLOG_TRACE, "[%s : %s : %d] max_hp [%d]", __FILE__, __FUNCTION__, __LINE__, r.max_hp);
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
