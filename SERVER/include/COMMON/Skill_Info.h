#pragma once
#include "common.h"
#include "PlayerData.h"

enum class AttackDir
{
    Left = -1,
    Right = 1,
};

enum class SkillCategory
{
    BASIC_ATTACK,
    ACTIVE,
    PASSIVE,
    NONE
};

enum class SkillCastType
{
    MELEE_ARC,
    PROJECTILE,
    AREA,
    NONE
};

enum class HitShape  
{
    NONE,
    ARC,
    BOX,
    CIRCLE,
    ENUMEND
};

enum class EffectType 
{
    NONE,
    KNOCKBACK, 
    ENUMEND
};

struct EffectDef
{
    EffectType type;
    float value; // KNOCKBACK이면 force, SLOW면 ratio 같은 식
};

struct SkillDef
{
    int skill_id;
    std::string key;

    SkillCategory category;
    SkillCastType cast_type;

    struct Hit
    {
        HitShape shape;
        float range;
        // Arc 전용
        float angle_deg;
        // BOX 전용
        float half_height = 0.0f;
        int max_targets;
        int hit_count;
    } hit;

    struct Damage
    {
        float multiplier;
        int flat_add;
    } damage;

    struct Requirements
    {
        RootJob root_job;
        int min_tier;
        int min_skill_level;
        
    } Requirements;

    int cooldown_ms;
    int mp_cost;

    std::vector<EffectDef> effects;
};

struct LearnedSkill
{
    // 나중에 확장을 위해서 구조체로 정의
    int skill_id;
    int skill_level;
};

struct LearnedSkillSlot
{
    int slot_index;
    int skill_id;
};

namespace Skill
{
    inline SkillCastType SetSkillCastType(std::string skillCastType )
    {
        if(skillCastType == "PROJECTILE") return SkillCastType::PROJECTILE;
        if(skillCastType == "MELEE_ARC") return SkillCastType::MELEE_ARC;
        if(skillCastType == "AREA") return SkillCastType::AREA;
        return SkillCastType::NONE;
    };

    inline SkillCategory SetSkillCategory(std::string skillCategory)
    {
        if(skillCategory == "BASIC_ATTACK") return SkillCategory::BASIC_ATTACK;
        if(skillCategory == "ACTIVE") return SkillCategory::ACTIVE;
        if(skillCategory == "PASSIVE") return SkillCategory::PASSIVE;
        return SkillCategory::NONE;
    }

    inline HitShape SetHitShape(std::string hitType)
    {
        if(hitType == "MELEE_ARC") 
        {
            K_LOG_TRACE( "ARC 공격 모양 설정.\n");
            return HitShape::ARC;
        }
        else
        {
            K_LOG_TRACE( "NONE 공격 모양 설정.\n");
            return HitShape::NONE;
        }
        return HitShape::NONE;
    };

    inline EffectType SetEffectType(std::string effectType)
    {
        if(effectType == "KNOCKBACK") return EffectType::KNOCKBACK;
        return EffectType::NONE;
    };

}
