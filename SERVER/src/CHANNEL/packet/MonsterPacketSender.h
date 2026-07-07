#pragma once

#include "common.h"
#include "Player.h"
#include "Monster.h"
#include "CommonEnum.h"

class MonsterPacketSender
{
public:
    static void SendMonsterSnapShot(Player* player, const std::vector<MonsterSnapshotInfo>& monsters);
    static void SendMonsterMove(Player* player, const std::vector<MonsterMoveInfo>& monsters);
    static void SendMonsterOnDamaged(Player* Attacker, int SkillID, std::vector<MonsterHitResult>& result, std::unordered_map<int, Player*>& playerList);
    static void SendMonsterRespawn(std::unordered_map<int, Player*>& playerList, const std::vector<MonsterRespawnInfo>& monsters);
private:
};