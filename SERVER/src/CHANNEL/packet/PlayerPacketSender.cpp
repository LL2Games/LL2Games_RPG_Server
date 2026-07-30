#include "PlayerPacketSender.h"
#include "ChannelSession.h"
#include "CharacterStat.h"
#include "K_slog.h"


void PlayerPacketSender::SendPlayerInfo(Player* player)
{
    auto session = player->GetSession();
    if(!session) 
    {
        K_LOG_ERROR( "session이 nullptr입니다.");
        return;
    }

    std::vector<std::string> payload;

    payload.push_back(std::to_string(player->GetId()));
    payload.push_back(player->GetName());
    payload.push_back(std::to_string(player->GetJob()));
    payload.push_back(std::to_string(player->GetLevel()));
    payload.push_back(std::to_string(player->GetMapId()));
    payload.push_back(std::to_string(player->GetPos().xPos));
    payload.push_back(std::to_string(player->GetPos().yPos));
    session->Send(PKT_PLAYER_INFO, payload);

    K_LOG_TRACE( "PlayerInfo Send Success.");
}


void PlayerPacketSender::SendPlayerStat(Player* player)
{
    if (player == nullptr)
        return;

    auto session = player->GetSession();

    if(!session) 
    {
        K_LOG_ERROR( "session이 nullptr입니다.");
        return;
    }

    auto playerStat = player->GetStatSnapShot();
    auto playerBastStat = playerStat.GetBase();
    std::vector<std::string> payload;

    // 나중에 스탯 정보에 공격력도 들어가야함
    payload.push_back(std::to_string(playerBastStat.str));
    payload.push_back(std::to_string(playerBastStat.dex));
    payload.push_back(std::to_string(playerBastStat.intel));
    payload.push_back(std::to_string(playerBastStat.luck));

    payload.push_back(std::to_string(playerStat.GetMaxHp()));
    payload.push_back(std::to_string(playerStat.GetMaxMp()));

    payload.push_back(std::to_string(playerStat.GetCurHp()));
    payload.push_back(std::to_string(playerStat.GetCurMp()));
    payload.push_back(std::to_string(playerStat.GetRemainAp()));
    payload.push_back(std::to_string(playerStat.GetLevel()));
    payload.push_back(std::to_string(playerStat.GetExp()));
    payload.push_back(std::to_string(playerStat.GetNeedExp()));
    session->Send(PKT_PLAYER_STAT, payload);

    K_LOG_TRACE( "SendPlayerStat Send Success.");
}



void PlayerPacketSender::SendPlayerSkillList(Player* player)
{
    auto session = player->GetSession();

    if(!session) 
    {
        K_LOG_ERROR( "session이 nullptr입니다.");
        return;
    }

    auto playerSkillList = player->GetPlayerSkillList();
    
    
    std::vector<std::string> payload;
    payload.push_back(std::to_string(playerSkillList.size()));
    // 나중에 스탯 정보에 공격력도 들어가야함
    for(auto skill : playerSkillList)
    {
        payload.push_back(std::to_string(skill.skill_id));
        payload.push_back(std::to_string(skill.skill_level));
    }
 
    session->Send(PKT_PLAYER_SKILLLIST, payload);
  
    K_LOG_TRACE( "SendPlayerSkillList Send Success.");
}


void PlayerPacketSender::SendPlayersMove(Player* sender, Vec2 pos, float speed, int dir, std::unordered_map<int, Player*>& playerList)
{
    std::vector<std::string> payload;
	
	payload.push_back(std::to_string(sender->GetId()));
	payload.push_back(std::to_string(pos.xPos));
	payload.push_back(std::to_string(pos.yPos));
	payload.push_back(std::to_string(speed));
    payload.push_back(std::to_string(dir));
    payload.push_back(std::to_string(static_cast<int>(sender->GetState())));

	for(auto it = playerList.begin(); it != playerList.end(); ++it)
	{
		if(it->second == sender) continue;
        if(it->second == nullptr) continue;
		
		auto session = it->second->GetSession();
        if(session == nullptr) continue;
		
		session->Send(PKT_PLAYER_MOVE, payload);
	}
}

void PlayerPacketSender::SendPlayerOnDamaged(Player* Defender, PlayerHitResult result, std::unordered_map<int, Player*>& playerList)
{
      for (auto& [id, p] : playerList)
    		{
        	if (!p) continue;

        	auto session = p->GetSession();
        	if (!session) continue;

        	std::vector<std::string> payload;
        	payload.reserve(6);

        	if (p == Defender)
        	{
        	    // 본인: 상세
        	    payload.push_back(std::to_string(result.player_id));
        	    payload.push_back(std::to_string(result.attacker_instance_id));
        	    payload.push_back(std::to_string(result.damage));
        	    payload.push_back(std::to_string(result.cur_hp));
        	    payload.push_back(std::to_string(result.max_hp));
        	    payload.push_back(std::to_string(static_cast<int>(result.state)));
        	}
        	else
        	{
        	    // 타인: 최소(HP 미공개)
        	    payload.push_back(std::to_string(result.player_id));
        	    payload.push_back(std::to_string(result.attacker_instance_id));
        	    payload.push_back(std::to_string(result.damage));
        	    payload.push_back(std::to_string(static_cast<int>(result.state)));
        	}

        	session->Send(PKT_PLAYER_ONDAMAGED, payload);
    }	
}

void PlayerPacketSender::SendExpGain(Player *player, const ExpResult& expResult)
{
    if (player == nullptr)
        return;

    auto session = player->GetSession();
    if (session == nullptr)
        return;

    std::vector<std::string> payload;
	
	payload.push_back(std::to_string(expResult.gainedExp));
	payload.push_back(std::to_string(expResult.newLevel));
	payload.push_back(std::to_string(expResult.curExp));
	payload.push_back(std::to_string(expResult.needExp));
    payload.push_back(std::to_string(static_cast<int>(expResult.levelUp)));

	session->Send(PKI_PLAYER_EXP_GAIN, payload);
    
}

void PlayerPacketSender::SendPlayerEnter(Player *player, std::unordered_map<int, Player *> &playerList)
{
    std::vector<std::string> payload;

    payload.push_back(std::to_string(player->GetId()));
    payload.push_back(player->GetName());
    payload.push_back(std::to_string(player->GetJob()));  
    payload.push_back(std::to_string(player->GetPos().xPos));
    payload.push_back(std::to_string(player->GetPos().yPos));
    payload.push_back(std::to_string(player->GetDir()));
    payload.push_back(std::to_string(static_cast<int>(player->GetState())));


    for(const auto& [id, otherPlayer] : playerList)
    {
        if(otherPlayer == player) continue;

        otherPlayer->GetSession()->Send(PKT_OTHERPLAYER_ENTER, payload);
        K_LOG_TRACE( "플레이어 Enter 정보 전달 완료.\n");
    }
}

void PlayerPacketSender::SendPlayerAttack(Player *attacker, int skillId, int attackDir, std::unordered_map<int, Player *> &playerList)
{
    std::vector<std::string> payload;
    payload.push_back(std::to_string(attacker->GetId()));
    payload.push_back(std::to_string(skillId));
    payload.push_back(std::to_string(attackDir));
    payload.push_back(std::to_string(3));

    for(const auto& [id, player] : playerList)
    {
        if(attacker == player) continue;

        player->GetSession()->Send(PKT_OTHER_PLAYER_ATTACK, payload);
    }
}

void PlayerPacketSender::SendExistingPlayersToNewPlayer(Player *newPlayer, std::unordered_map<int, Player *> &playerList)
{
    std::vector<std::string> payload;

    int count = 0;
    for (const auto& [id, other] : playerList)
    {
        if (other != nullptr && other != newPlayer)
            count++;
    }

    payload.push_back(std::to_string(count));

    for (const auto& [id, other] : playerList)
    {
        if (other == nullptr || other == newPlayer)
            continue;

        payload.push_back(std::to_string(other->GetId()));
        payload.push_back(other->GetName());
        payload.push_back(std::to_string(other->GetJob()));
        payload.push_back(std::to_string(other->GetPos().xPos));
        payload.push_back(std::to_string(other->GetPos().yPos));
        payload.push_back(std::to_string(other->GetDir()));
        payload.push_back(std::to_string(static_cast<int>(other->GetState())));
    }

    newPlayer->GetSession()->Send(PKT_OTHERPLAYER_SNAPSHOT, payload);
}
