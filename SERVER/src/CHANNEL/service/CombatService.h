#pragma once

#include "common.h"
#include "SkillManager.h"
#include "MapInstance.h"
#include "Player.h"
#include "Monster.h"
#include "Collider.h"


class CombatService
{
public:
    CombatService();
    ~CombatService();

    int HandleSkillAttack(Player* Attacker, int skill_id, int attack_dir);
    int HandleBasicAttack(Player* Attacker, int attack_dir);
    int ApplyContactDamage(Player* player, Monster& monster);
    int CalculateSkillBaseDamage(const Player* attacker, const SkillDef& skillDef); 

    int GetLevelDiffRate(int attackerLevel, int DefenderLevel) const;
    // 함수 뒤에 const를 붙이면 멤버 변수를 변경하지 않겠다는 의미 : 함수 안에서 멤버 변수 값을 변경한다면 에러가 발생
    int CalculateFinalDamage(int baseDmg, const Player* Attacker, const SkillDef& skillDef, const Monster& m) const;
    int CalculateFinalDamage(Player* player, Monster& m) const;

    
    std::vector<Monster*> ComputeHitMonsters(Player* attacker, std::vector<Monster>& monsters, const SkillDef& skillDef, int attack_dir);
private:
    static constexpr int kLevelRateMinPct = 30;
    static constexpr int kLevelRateMaxPct = 120;
    static constexpr int kLevelRateBasePct = 100;

    // 레벨 1 차이당 변동량(퍼센트 포인트). 0.5%p면 정수로 5/10 추천
    static constexpr int kLevelRateStepNum = 5;   // 분자
    static constexpr int kLevelRateStepDen = 10;  // 분모

    // (플레이어가 훨 높을 때) 몬스터 몸박/공격 약화 임계값
    static constexpr int kContactPenaltyLevelDiff = 30;

    // 임계값 넘으면 배율을 이 값으로 고정(예: 5%)
    static constexpr int kContactPenaltyRatePct = 5;
private:

    SkillManager* m_skillManager = nullptr;


};