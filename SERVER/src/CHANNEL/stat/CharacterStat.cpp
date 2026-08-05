#include "CharacterStat.h"
#include "K_slog.h"
#include "LevelManager.h"


CharacterStat::CharacterStat() : m_base{},
      m_derived{},
      m_expStat{},
      m_cur_hp(0),
      m_cur_mp(0),
      m_remain_ap(0)
{

}

ExpResult CharacterStat::AddExp(int64_t exp)
{
    ExpResult result{};  
    if (exp <= 0)
        return result;

    result.gainedExp = exp;
    result.oldLevel = m_expStat.level;
     
    m_expStat.exp += exp;
      
    LevelManager* levelManager = LevelManager::GetInstance();

    while (true)
    {
        int64_t needExp = levelManager->GetNeedExp(m_expStat.level);

        if (needExp <= 0)
            break;

        if (m_expStat.exp < needExp)
            break;

        m_expStat.exp -= needExp;
        m_expStat.level++;

        m_remain_ap += m_levelUp_ap;
        m_cur_hp = m_derived.maxHp;
        m_cur_mp = m_derived.maxMp;
    }

    result.newLevel = m_expStat.level;
    result.curExp = m_expStat.exp;
    result.needExp = levelManager->GetNeedExp(m_expStat.level);
    result.remainAp = m_remain_ap;

    result.curHp = m_cur_hp;
    result.maxHp = m_derived.maxHp;
    result.curMp = m_cur_mp;
    result.maxMp = m_derived.maxMp;

    result.levelUp = result.newLevel > result.oldLevel;

    return result;
}

void CharacterStat::Up(const std::string &statType)
{
    if(m_remain_ap <= 0) return;
    int statNum = 0;
    const static std::string stats[4] = {"str", "dex", "intel", "luck"};
    for (int i = 0; i < 4; i++)
    {
        if (stats[i] == statType)
        {
            statNum = i;
            break;
        }
    }

    switch (statNum)
    {
    case E_STR:
        m_base.str++;
        m_remain_ap--;
        break;

    case E_DEX:
        m_base.dex++;
        m_remain_ap--;
        break;

    case E_INT:
        m_base.intel++;
        m_remain_ap--;
        break;

    case E_LUCK:
        m_base.luck++;
        m_remain_ap--;
        break;
    }

    K_LOG_DEBUG( "stat[%s] up", statType.c_str());
}