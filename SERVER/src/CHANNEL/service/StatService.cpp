#include "StatService.h"
#include <string>

StatService::StatService()
{

}

StatService::~StatService()
{

}

int StatService::UpStat(Player &player, const std::string &statType, std::string &errMsg)
{
    int result = m_repo.Update(std::to_string(player.GetId()), statType, errMsg);
    if (result != 0)
        return result;

    player.UpStat(statType);
    return 0;
}

int StatService::SaveRuntimeStat(Player& player, std::string& errMsg)
{
    if (!player.IsStatDirty())
        return 0;

    CharacterStat stat = player.GetStatSnapShot();

    int result = m_repo.SaveRuntimeStat(player.GetId(), stat, errMsg);
    if (result == 0)
        player.ClearStatDirty();

    return result;
}