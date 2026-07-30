#pragma once
#include "Player.h"
#include "CharacterStat.h"
#include "PlayerStatRepository.h"

class StatService
{
public:
	StatService();
	~StatService();

	int UpStat(Player &player, const std::string &statType, std::string &errMsg);
	int SaveRuntimeStat(Player& player, std::string& errMsg);
private:
	PlayerStatRepository m_repo;
	
};
