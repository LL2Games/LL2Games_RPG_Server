#pragma once
#include <string>
#include "PlayerData.h"
#include "StatInfoPacket.h"
enum e_BaseStat
{
    E_STR = 0,
    E_DEX,
    E_INT,
    E_LUCK,
};

struct BaseStat
{
    int str;
    int dex;
    int intel;
    int luck;
};

struct DerivedStat
{
    int maxHp;
    int maxMp;
};

struct ExpStat
{
    int level;
    int64_t exp;
    int64_t need_exp;
};

struct AllStat
{
    BaseStat baseStat{};
    DerivedStat derivedStat{};
    ExpStat expStat{};

    int curHp;
    int curMp;
    int remainAp;
};

struct MainSubStat
{
    e_BaseStat main;
    e_BaseStat sub;
};

inline MainSubStat GetMainSubStat(RootJob job)
{
    switch(job)
    {
        case RootJob::Knight:   return {e_BaseStat::E_STR, e_BaseStat::E_DEX};
        case RootJob::Archer:   return {e_BaseStat::E_DEX, e_BaseStat::E_STR};
        case RootJob::Mage:     return {e_BaseStat::E_INT, e_BaseStat::E_LUCK};
        case RootJob::Thief:    return {e_BaseStat::E_LUCK, e_BaseStat::E_DEX};
        default:                return {e_BaseStat::E_STR, e_BaseStat::E_DEX};
    }
}

inline int GetStatValue(const BaseStat& s, e_BaseStat t)
{
    switch (t)
    {
        case e_BaseStat::E_STR: return s.str;
        case e_BaseStat::E_DEX: return s.dex;
        case e_BaseStat::E_INT: return s.intel;
        case e_BaseStat::E_LUCK: return s.luck;
        default:            return 0;
    }
}


class CharacterStat {

public:
    CharacterStat(){}
    CharacterStat(const BaseStat& base, const DerivedStat& derived, const ExpStat& expStat, int curHp, int curMp, int remainAp)
    :m_base(base), m_derived(derived), m_expStat(expStat), m_cur_hp(curHp), m_cur_mp(curMp), m_remain_ap(remainAp)
    {}

    //조회용 인터페이스
    const BaseStat& GetBase() const { return m_base; }
    const DerivedStat& GetDerived() const {return m_derived; }

    int& GetCurHp() {return m_cur_hp;}
    int GetCurHp() const {return m_cur_hp;}

    int& GetCurMp() {return m_cur_mp;}
    int GetCurMp() const {return m_cur_mp;}

    int GetMaxHp() const {return m_derived.maxHp;}
    int GetMaxMp() const {return m_derived.maxMp;}
    int GetRemainAp() const {return m_remain_ap;}

    void SetCurMp(int cur_mp) {m_cur_mp = cur_mp;}
    void SetCurHp(int cur_hp) {m_cur_hp = cur_hp;}
    
    int GetLevel() const {return m_expStat.level;}
    int64_t GetExp() const {return m_expStat.exp;}
    int64_t GetNeedExp() const {return m_expStat.need_exp;}
    
public:
    void Up(const std::string & statType);
    ExpResult AddExp(int64_t exp);
private:
    BaseStat m_base;
    DerivedStat m_derived;
    ExpStat m_expStat;
    int m_cur_hp;
    int m_cur_mp;
    int m_remain_ap;
private:
    static constexpr u_int16_t m_levelUp_ap = 5;

    
};