#include "CombatService.h"
#include "CommonEnum.h"
#include "Math.h"
#include "PlayerData.h"
#include "MapInstance.h"
#include "Collider.h"

CombatService::CombatService()
{
    m_skillManager = SkillManager::GetInstance();
}

CombatService::~CombatService()
{
    
}


int CombatService::HandleAttack(Player* Attacker, int skill_id, int attack_dir)
{
    MapInstance* map = Attacker->GetCurrentMap();

    std::optional<SkillDef> skillDef = m_skillManager->GetSkill(skill_id);

    if(!skillDef) return 0;

    //캐릭터가 공격 가능 상태인지 확인
    if(!Attacker->CanAttack(&*skillDef))
    {
        K_LOG_TRACE( "플레이어가 공격 가능한 상태가 아닙니다.\n");
        return 0;
    }

    // 플레이어 스킬 사용
    Attacker->UseSkill(&*skillDef);

    // MapInstance에서 해당 맵에 스폰된 몬스터들의 정보를 추출
    std::vector<Monster>& monster_list = map->GetMonsterList();
     
    K_LOG_TRACE( "monster_list size = %zu\n", monster_list.size());



#if 0 /*gunoo22 260223 맵내 모든 몬스터 피격 테스트*/
    std::vector<Monster*> HitMonsters;
    for (auto& m : monster_list)
    {
        HitMonsters.push_back(&m);
    }
    K_LOG_TRACE( "테스트 HitMonsters size[%d]", HitMonsters.size());

    (void)attack_dir; // 현재 테스트에서는 공격 방향 무시
#else
    // ComputeHitMonsters(Player* attacker, const std::vector<Monster>& monsters, const SkillDef& skillDef, std::string attack_dir)
    // 몬스터들 중에서 피격된 몬스터들을 가져온다.
    std::vector<Monster*> HitMonsters = ComputeHitMonsters(Attacker, monster_list, *skillDef, attack_dir);
#endif

    K_LOG_TRACE( "HitMonsters size = %zu\n", HitMonsters.size());


    std::vector<std::pair<Monster*, int>> hitResults;
    hitResults.reserve(HitMonsters.size());

    // 플레이어 스킬 레벨 및 주스탯 / 보조스탯에 따라서 데미지를 계산
    int skillDamage = CalculateSkillBaseDamage(Attacker, *skillDef);

    for(Monster* monster : HitMonsters)
    {
        int finalDamage = CalculateFinalDamage(skillDamage,Attacker, *skillDef, *monster);
        //monster->
        hitResults.push_back({monster, finalDamage});
    }
    
    map->ResolveSkillHit(Attacker, *skillDef, hitResults);
  
    return 1;

}

int CombatService::HandleBasicAttack(Player* Attacker, int attack_dir)
{
    MapInstance* map = Attacker->GetCurrentMap();

    std::optional<SkillDef> skillDef = m_skillManager->GetSkill(static_cast<int>(Attacker->GetWeaponType()));
    K_LOG_TRACE( "[%d] 기본공격 시도 중.\n", static_cast<int>(Attacker->GetWeaponType()));
    if(!skillDef) 
    {
        
        K_LOG_TRACE( "기본공격 데이터가 없습니다.\n");
        return 0;
    }
    //캐릭터가 공격 가능 상태인지 확인
    if(!Attacker->CanAttack(&*skillDef))
    {
        K_LOG_TRACE( "플레이어가 공격 가능한 상태가 아닙니다.\n");
        return 0;
    }

    // 플레이어 스킬 사용
    Attacker->UseSkill(&*skillDef);

    // MapInstance에서 해당 맵에 스폰된 몬스터들의 정보를 추출
    std::vector<Monster>& monster_list = map->GetMonsterList();
     
    // ComputeHitMonsters(Player* attacker, const std::vector<Monster>& monsters, const SkillDef& skillDef, std::string attack_dir)
    // 몬스터들 중에서 피격된 몬스터들을 가져온다.
    std::vector<Monster*> HitMonsters = ComputeHitMonsters(Attacker, monster_list, *skillDef, attack_dir);

    std::vector<std::pair<Monster*, int>> hitResults;
    hitResults.reserve(HitMonsters.size());

    // 플레이어 스킬 레벨 및 주스탯 / 보조스탯에 따라서 데미지를 계산
    int skillDamage = CalculateSkillBaseDamage(Attacker, *skillDef);

    for(Monster* monster : HitMonsters)
    {
        int finalDamage = CalculateFinalDamage(skillDamage,Attacker, *skillDef, *monster);
        hitResults.push_back({monster, finalDamage});
    }
    
    map->ResolveSkillHit(Attacker, *skillDef, hitResults);
  
    return 1;
}


std::vector<Monster*> CombatService::ComputeHitMonsters(Player* attacker, std::vector<Monster>& monsters, const SkillDef& skillDef, int attack_dir)
{
    std::vector<Cand> canHitMonsters;
    std::vector<Monster*> hitMonsters;

    if (attacker == nullptr)
        return hitMonsters;

    if (attack_dir != -1 && attack_dir != 1)
        return hitMonsters;

    Vec2 attackerPos = attacker->GetPos();

    canHitMonsters.reserve(monsters.size());

    //gunoo22 260226 몬스터 체력 안다는 이슈 -> 레퍼런스로 안받음
    for (Monster& m : monsters) 
    {
        if(!m.IsAlive()) continue;
        bool isHit = false;
        const Collider2D& monsterCollider = m.GetCollider();
  
        if (skillDef.hit.shape == HitShape::BOX)
        {
            Collider2D attackCollider = Collision::MakeBoxAttackCollider(skillDef, attack_dir);
        
            isHit = Collision::Intersects(
                attackerPos,
                attackCollider,
                m.GetPos(),
                monsterCollider
            );
        }
        else if (skillDef.hit.shape == HitShape::ARC)
        {
            K_LOG_TRACE( "ARC 공격 타입.\n");
            isHit = Collision::IntersectArc2D(
                attackerPos,
                attack_dir,
                skillDef.hit.range,
                skillDef.hit.angle_deg,
                m.GetPos(),
                monsterCollider
            );
        }
        else
        {
            K_LOG_TRACE( "이상 공격 타입.\n");
        }
        if (!isHit)
            continue;
    
        Vec2 monsterCenter = Collision::GetColliderCenter(
            m.GetPos(),
            monsterCollider
        );

        float dx = monsterCenter.xPos - attackerPos.xPos;
        float front = dx * static_cast<float>(attack_dir);

        canHitMonsters.push_back({ &m, front });
    }

    std::sort(canHitMonsters.begin(), canHitMonsters.end(),
        [](const Cand& a, const Cand& b)
        {
            if (a.front < b.front) return true;
            if (a.front > b.front) return false;
            return a.m->GetInstanceId() < b.m->GetInstanceId();
        });

    int take = std::min<int>(
        skillDef.hit.max_targets,
        static_cast<int>(canHitMonsters.size())
    );

    hitMonsters.reserve(take);

    for (int i = 0; i < take; ++i)
    {
        hitMonsters.push_back(canHitMonsters[i].m);
    }

    return hitMonsters;

}

int CombatService::CalculateSkillBaseDamage(const Player* attacker, const SkillDef& skillDef) 
{
    BaseStat attackerStat = attacker->GetStat().GetBase();

    auto ms = GetMainSubStat(attacker->GetRootJob());
    int mainStat = GetStatValue(attackerStat, ms.main);
    int subStat  = GetStatValue(attackerStat, ms.sub);

    int skillLevel = attacker->GetSkillLevel(skillDef.skill_id);
    if (skillLevel < 1) skillLevel = 1;

    float statBase  = static_cast<float>(mainStat) * 4.0f + static_cast<float>(subStat);
    float levelRate = 1.0f + 0.05f * static_cast<float>(skillLevel - 1);

    float dmg = statBase * levelRate * skillDef.damage.multiplier;
    dmg += static_cast<float>(skillDef.damage.flat_add);
    K_LOG_TRACE( "Damaged [%f]", dmg);
    if (dmg < 1.0f) dmg = 1.0f;
    return static_cast<int>(dmg);
}


int CombatService::ApplyContactDamage(Player* player, Monster& monster)
{
    // monster와 player의 레벨 차이와 플레이어의 방어력 등을 기반으로 데미지 계산
    int finalDamage = CalculateFinalDamage(player, monster);
    
    return finalDamage;
}

// 콜리전이 부닺혔을 때 데미지 계산
int CombatService::CalculateFinalDamage(Player* player, Monster& m) const
{
    int dmg = m.GetDamage();

    int lvRate = GetLevelDiffRate(m.GetLevel(), player->GetLevel());
    dmg = dmg * lvRate / kLevelRateBasePct;

    // 방어/저항 등 추가 보정
    // dmg = ApplyDefense(dmg, m);
    // dmg = ApplyElement(dmg, skillDef, m);

    if (dmg < 1) dmg = 1;
    return dmg;
}

// 스킬로 공격 했을 때 데미지 계산
int CombatService::CalculateFinalDamage(int baseDmg, const Player* Attacker, const SkillDef& skillDef, const Monster& m) const
{
    (void)skillDef;
    int dmg = baseDmg;

    int lvRate = GetLevelDiffRate(Attacker->GetLevel(), m.GetLevel());
    dmg = dmg * lvRate / kLevelRateBasePct;

    // 방어/저항 등 추가 보정
    // dmg = ApplyDefense(dmg, m);
    // dmg = ApplyElement(dmg, skillDef, m);

    if (dmg < 1) dmg = 1;
    return dmg;
}

int CombatService::GetLevelDiffRate(int AttackerLevel, int DefenderLevel) const
{
    int rate = 0;
    int diff = AttackerLevel - DefenderLevel;
    
    // 공격 받는 자와 공격자의 레벨 차가 크면 데미지 비율 고정
    if(DefenderLevel - AttackerLevel >=  kContactPenaltyLevelDiff) {
        return kContactPenaltyRatePct;
    }
    rate = kLevelRateBasePct + diff * kLevelRateStepNum / kLevelRateStepDen;
    if(rate < kLevelRateMinPct) rate = kLevelRateMinPct;
    else if(rate > kLevelRateMaxPct) rate = kLevelRateMaxPct;
    return rate;
}

