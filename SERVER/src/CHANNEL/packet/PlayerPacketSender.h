#include "common.h"
#include "Player.h"
#include "StatInfoPacket.h"
class PlayerPacketSender
{
public:
    static void SendPlayerInfo(Player* player);
    static void SendPlayerStat(Player* player);
    static void SendPlayerSkillList(Player* player);
    static void SendPlayersMove(Player* sender, Vec2 pos, float speed, int dir, std::unordered_map<int, Player*>& playerList);
    static void SendPlayerAttack(Player* attacker, int skillId, int  attackDir, std::unordered_map<int, Player*>& playerList);
    static void SendPlayerOnDamaged(Player* Defender, PlayerHitResult result, std::unordered_map<int, Player*>& playerList);
    static void SendExpGain(Player* player, const ExpResult& expResult);
    static void SendPlayerEnter(Player* player, std::unordered_map<int, Player*>& playerList);
    static void SendPlayerLeave(int playerId,const std::unordered_map<int, Player*>& playerList);
    static void SendExistingPlayersToNewPlayer(Player* newPlayer ,std::unordered_map<int, Player*>& playerList);
private:


};