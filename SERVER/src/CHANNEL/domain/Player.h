#pragma once
#include "common.h"
#include "CommonEnum.h"
#include "PlayerData.h"
#include "CharacterStat.h"
#include "InventoryManager.h"
#include "PlayerState.h"
#include "PlayerData.h"
#include "Skill_Info.h"
#include "time.h"
#include "Collider.h"
#include "SkillManager.h"
#include "QuickSlotManager.h"
#include "Weapon_Info.h"
#include "StatInfoPacket.h"
#include <mutex>

class ChannelSession;
class MapInstance;

struct PlayerHitResult {
    int player_id;
    int attacker_instance_id;   // 몬스터/플레이어 구분 가능한 id 체계면 좋다
    int damage;
    int cur_hp;
    int max_hp;
    PlayerState state;
};

class Player
{
public:
    Player();
    ~Player();

public: 
    void SetId(int m_char_id){this->m_char_id = m_char_id;}
    void SetAccountId(std::string m_account_id) {this->m_account_id = m_account_id;}
    void SetName(std::string m_name){ this->m_name = m_name;}
    
    void SetJob(int m_job){this->m_job = m_job;}
    void SetMapId(int m_map_id){this->m_map_id = m_map_id;}

    void SetLevel(int m_level){this->m_level = m_level;}

    void SetPos(float m_xPos, float m_yPos) {this->m_xPos = m_xPos; this->m_yPos = m_yPos;}\
    void SetPos(Vec2 Pos) {m_xPos = Pos.xPos, m_yPos = Pos.yPos;}

    void SetInitData(const PlayerInitData playerInitData);
    void SetInitData(const PlayerInitData playerInitData, const CharacterStat &stat);

    void SetCurrentMap(MapInstance* map) {m_current_map = map;}
    void SetSession(ChannelSession* session) {m_session = session;}

    void SetLearnedSkill(const LearnedSkill& learnedSkill){m_learnedSkills[learnedSkill.skill_id] = learnedSkill;}

    void SetState(PlayerState state) {m_CurrentState = state;}

public:
    int GetCurHP() const
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        return m_stat.GetCurHp();
    }
    int GetCurMP() const
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        return m_stat.GetCurMp();
    }
    int GetMaxHP() const
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        return m_stat.GetMaxHp();
    }
    PlayerState GetState() const
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        return m_CurrentState;
    }
    bool IsAlive() const
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        return m_CurrentState != PlayerState::DEAD ? true : false;
    }

    CharacterStat GetStatSnapShot() const
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        return m_stat;
    }
    BaseStat GetBaseStatSnapshot() const
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        return m_stat.GetBase();
    }

    bool IsStatDirty() const
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        return m_statDirty;
    }
    
    void ClearStatDirty()
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        m_statDirty = false;
    }

    int GetJob() {return m_job;}
    int GetId() {return m_char_id;}
    int GetMapId() {return m_map_id;}
    int GetLevel() const {return m_level;}
    int GetSkillLevel(int skill_id) const;
    WeaponType GetWeaponType() const {return m_weaponType;}
    

    InventoryManager* GetInventoryManager() {return &m_inventoryManager;}
    SkillManager* GetSkillManager() {return m_skillManager;}
    QuickSlotManager* GetQuickSlotManager() {return &m_quickSlotManager;}

    

    std::string GetName() {return m_name;}

    MapInstance* GetCurrentMap() {return m_current_map;}
    ChannelSession* GetSession() {return m_session;}

    //stat
    CharacterStat& GetStat() {return m_stat;}
    const CharacterStat& GetStat() const {return m_stat;}    
  
    Vec2 GetPos() {return Vec2{m_xPos, m_yPos};}
    RootJob GetRootJob() const {return m_root_job;}
    Collider2D GetCollider() {return m_collider;}

    std::vector<LearnedSkill> GetPlayerSkillList() const;

    int GetDir() {return m_dir;}

public:

    // 현재 플레이어가 공격 가능 상태인지 확인한다.
    bool CanAttack(SkillDef* skillDef);

    bool CanUseItem(int inventoryType, int slotPos, int item_id, int useCount);
    bool UseItem(int inventoryType, int slotPos, int itemId, int useCount);

    void UseSkill(SkillDef* skillDef);
    void UpStat(const std::string& statType);
    void AddHP(int HP);
    void AddMP(int MP);

    int GetItemCount(int inventoryType, int slotPos, int itemId) const;

    // 현재 공격을 받을 수 있는 상태인지 확인하는 함수 : 메이플에서 피격 당할 시 1초 정도의 무적이 존재하기 때문에 확인 함수 필요
    bool CanTakeAnyContactDamage(int64_t nowMs);

    // 플레이어가 피격 당했을 때 처리하는 함수
    void OnDamaged(int dmg,int64_t nowMs);

    void Dead();

    ExpResult AddExp(int64_t  exp);

private:
    int m_char_id;
    std::string m_account_id;
    std::string m_name;
   
    RootJob m_root_job;
    WeaponType m_weaponType = WeaponType::One_Hand;

    int m_level;
    int m_job;
    int m_map_id;
    float m_xPos;
    float m_yPos;
    int m_dir;

    Collider2D m_collider;

    MapInstance* m_current_map;
    ChannelSession* m_session;

    PlayerState m_CurrentState;

    // 플레이어가 배운 스킬들 저장 key는 skillID,
    std::unordered_map<int , LearnedSkill> m_learnedSkills;
    // 플레이어가 설정한 스킬 슬롯
    std::unordered_map<int,int> m_skillSlots;
    // 사용한 스킬쿨들을 저장
    std::unordered_map<int, int64_t> skillCooldownEndMs;


private:
    bool m_isChangedInventory;
    bool m_isChangedStatas;
    bool m_isChangedPositon;
    bool m_statDirty = false;

    CharacterStat m_stat;
    InventoryManager m_inventoryManager;
    SkillManager* m_skillManager;
    QuickSlotManager m_quickSlotManager;
   
    int64_t m_nextContactDamageAllowedMs;
    int64_t m_contactDamageCooldownMs;

    mutable std::mutex m_statMutex;
};
