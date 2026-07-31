#include "Player.h"
#include "timeUtility.h"
#include "ItemManager.h"


Player::Player() : m_char_id(0), 
                   m_name(""), 
                   m_current_map(nullptr), 
                   m_session(nullptr), 
                   m_skillManager(SkillManager::GetInstance()),
                   m_nextContactDamageAllowedMs(-1),
                   m_contactDamageCooldownMs(1000) //무적 쿨타임
{
    //Player Collider 초기화 부분
    m_collider.type = ColliderType::Rect2D;
    // 일단 콜라이더 offset과 halfW, halfH 고정으로 설정 나중에 리소스 크기에 따라서 변경 해야함
    m_collider.rect.offset = {-6.f, -20.f};
    m_collider.rect.halfW = 18.f;
    m_collider.rect.halfH = 30.f;
    m_quickSlotManager.Init();
}

Player::~Player()
{
}

void Player::SetInitData(const PlayerInitData playerInitData)
{
    this->m_char_id = playerInitData.char_id;
    this->m_account_id = playerInitData.account_id;
    this->m_name = playerInitData.name;
    this->m_level = playerInitData.level;
    this->m_job = playerInitData.job;
    this->m_root_job = PlayerData::ToRootJob(playerInitData.root_job);
    this->m_map_id = playerInitData.map_id;
    this->m_xPos = playerInitData.xPos;
    this->m_yPos = playerInitData.yPos;

}

void Player::SetInitData(const PlayerInitData playerInitData, const CharacterStat &stat)
{
    this->m_char_id = playerInitData.char_id;
    this->m_account_id = playerInitData.account_id;
    this->m_name = playerInitData.name;
    this->m_level = playerInitData.level;
    this->m_job = playerInitData.job;
    this->m_root_job = PlayerData::ToRootJob(playerInitData.root_job);
    this->m_map_id = playerInitData.map_id;
    this->m_xPos = playerInitData.xPos;
    this->m_yPos = playerInitData.yPos;
    this->m_stat = stat;


#if 1 //테스트용
    /*ITEM 사용 테스트를 위한 Inven 채우기 */
    //m_inven[2000000] = 100;

    //HP물약회복 테스트를 위한 HP 임시세팅
    //m_stat.GetCurHp() = 10;

    //스킬테스트를 위한 임시데이터 삽입
    m_learnedSkills[20001].skill_level = 1; //스킬ID 20001을 레벨 1로 배웠다고 가정
#endif  

#if 0 /*gunoo22 260219 테스트로그*/
    K_LOG_DEBUG( "gunoo22_TEST Player SetInitData");
    K_LOG_DEBUG( "gunoo22_TEST char_id[%d]", playerInitData.char_id);
    K_LOG_DEBUG( "gunoo22_TEST account_id[%s]", playerInitData.account_id.c_str());
    K_LOG_DEBUG( "gunoo22_TEST name[%s]", playerInitData.name.c_str());
    K_LOG_DEBUG( "gunoo22_TEST level[%d]", playerInitData.level);
    K_LOG_DEBUG( "gunoo22_TEST job[%d]", playerInitData.job);
    K_LOG_DEBUG( "gunoo22_TEST root_job[%d]", playerInitData.root_job);
    K_LOG_DEBUG( "gunoo22_TEST map_id[%d]", playerInitData.map_id);
    K_LOG_DEBUG( "gunoo22_TEST xPos[%f]", playerInitData.xPos);
    K_LOG_DEBUG( "gunoo22_TEST yPos[%f]", playerInitData.yPos);
#endif
}


bool Player::CanUseSkill(SkillDef* skillDef)
{
    if (skillDef == nullptr)
    {
        K_LOG_TRACE( "skillDef is null\n");
        return false;
    }

    // 먼저 플레이어가 공격 가능한 상태인지 확인
    if (m_CurrentState == PlayerState::STUNNED)
    {
        K_LOG_TRACE( "현재 스턴 상태 입니다. 플레이어가 공격 가능한 상태가 아닙니다.\n");
        return false;
    }

    const bool isBasicAttack = (skillDef->category == SkillCategory::BASIC_ATTACK);

    K_LOG_TRACE( "skillDef->categoy [%d].\n", skillDef->category);

    // 기본 공격이 아닌 경우에만 직업/습득 여부 검사
    if (!isBasicAttack)
    {
#if 1 //gunoo22 260729 스킬 사용부분 확인을 위해 주석 실제 운영시 주석 풀어야함

        if (m_root_job != skillDef->Requirements.root_job)
        {
            K_LOG_TRACE( "플레이어가 사용 가능한 스킬이 아닙니다.\n");
            return false;
        }

        auto skillit = m_learnedSkills.find(skillDef->skill_id);
        if (skillit == m_learnedSkills.end())
        {
            K_LOG_TRACE( "플레이어가 배우지 않은 스킬입니다.\n");
            return false;
        }
#endif
    }

    // 마나 검사
    {
        std::lock_guard<std::mutex> lock(m_statMutex);
        if (m_stat.GetCurMp() < skillDef->mp_cost)
        {
            K_LOG_TRACE( "마나가 부족합니다.\n");
            return false;
        }
    }

    // 쿨타임 검사
    const int64_t now = NowMs();

    auto coolit = skillCooldownEndMs.find(skillDef->skill_id);
    if (coolit != skillCooldownEndMs.end() && now <= coolit->second)
    {
        K_LOG_TRACE( "아직 쿨타임입니다.\n");
        return false;
    }

    return true;

}

void Player::UseSkill(SkillDef* skillDef)
{
    std::lock_guard<std::mutex> lock(m_statMutex);
    int cur_mp = 0;
    int64_t now = NowMs();
    skillCooldownEndMs[skillDef->skill_id] = now + skillDef->cooldown_ms;

    cur_mp = m_stat.GetCurMp();
    cur_mp -= skillDef->mp_cost;

    m_stat.SetCurMp(cur_mp);

    K_LOG_TRACE( "CurMp =%d\n", cur_mp);
}

void Player::UpStat(const std::string& statType)
{
    std::lock_guard<std::mutex> lock(m_statMutex);
    m_stat.Up(statType);
}


bool Player::CanUseItem(int inventoryType, int slotPos, int item_id, int useCount)
{
    //인벤토리에서 해당 아이템이 있는지 확인

    //개수 체크

    // 아이템 타입 확인
    K_LOG_DEBUG( "아이템 사용 개수 [%d]", useCount);
    if (useCount <= 0) {
        K_LOG_ERROR( "invalid useCount=%d\n", useCount);
        return false;
    }

    // 아이템 정의 존재/타입 체크
    const ItemInitData* def = ItemManager::GetInstance()->Find(item_id);

    if (!def) {
        K_LOG_ERROR( "unknown item_id=%d\n", item_id);
        return false;
    }
    if (def->type != "consumable") {
        K_LOG_ERROR( "not consumable item_id=%d type=%s\n", item_id, def->type.c_str());
        return false;
    }

    auto inventory = m_inventoryManager.GetInventory(inventoryType);

    if(!inventory->HasItemBySlot(slotPos, item_id, useCount))
    {
        K_LOG_ERROR( "아이템을 사용할 수 없습니다. item_id=%d type=%s\n", item_id, def->type.c_str());
        return false;
    }

    return true;
}

bool Player::UseItem(int inventoryType, int slotPos, int itemId, int useCount)
{
   // 방어(실수 방지)
    if (useCount <= 0) return false;

    K_LOG_DEBUG( "useCount = %d\n", useCount);

    const ItemInitData* def = ItemManager::GetInstance()->Find(itemId);
    if (!def) return false;

    auto inventory = m_inventoryManager.GetInventory(inventoryType);

    if(!inventory->RemoveItemBySlot(slotPos, itemId, useCount))
    {
        K_LOG_ERROR( "아이템을 사용할 수 없습니다. item_id=%d\n", itemId);
        return false;
    }

    // 효과 적용
    // (ItemInitData.use_effect 타입에 따라 아래 분기만 맞추면 됨)
    if (def->use_effect)
    {
        // 예: hp/mp 회복
        const int hpAdd = def->use_effect->hp_restore * useCount;
        const int mpAdd = def->use_effect->mp_restore * useCount;

        AddHP(hpAdd);
        AddMP(mpAdd);

        // 쿨타임도 쓸 거면 여기서 등록
        // SetItemCooldown(itemId, def->use_effect->cooldown_ms);
    }

    return true;

}

int Player::GetItemCount(int inventoryType, int slotPos, int itemId) const
{
    
    auto inventory = m_inventoryManager.GetInventory(inventoryType);
    if(!inventory->HasItemBySlot(slotPos, itemId))
    {
        K_LOG_TRACE( "플레이어가 소유하지 않은 아이템입니다.\n");
        return 0;
    }
    return inventory->GetItemCount(slotPos, itemId);
}

int Player::GetSkillLevel(int skill_id) const
{
    auto it = m_learnedSkills.find(skill_id);

    if(it == m_learnedSkills.end())
    {
        K_LOG_TRACE( "배우지 않은 스킬입니다.\n");
        return 0;
    }

    return it->second.skill_level;
}

std::vector<LearnedSkill> Player::GetPlayerSkillList() const
{
    std::vector<LearnedSkill> learnedSkills;

    learnedSkills.reserve(m_learnedSkills.size());

    for(auto [id, skill] : m_learnedSkills)
    {
        learnedSkills.push_back(skill);
    }

    return learnedSkills;
}


void Player::AddHP(int HP)
{
    std::lock_guard<std::mutex> lock(m_statMutex);
    K_LOG_TRACE( "현재 체력 [%d].\n", m_stat.GetCurHp());
    K_LOG_TRACE( "추가 체력 [%d].\n", HP);
    m_stat.GetCurHp() += HP;
    if (m_stat.GetCurHp() > m_stat.GetMaxHp()) m_stat.GetCurHp() = m_stat.GetMaxHp();
    // if (m_stat.GetCurHp() < 0) m_stat.GetCurHp() = 0; //체력깎일때 조건사용 ex)OnDamage에서 HP 감소할 때
    m_statDirty = true;
}

void Player::AddMP(int MP)
{
    std::lock_guard<std::mutex> lock(m_statMutex);
    m_stat.GetCurMp() += MP;
    if (m_stat.GetCurMp() > m_stat.GetMaxMp()) m_stat.GetCurMp() = m_stat.GetMaxMp();
    // if (m_stat.GetCurMp() < 0) m_stat.GetCurMp() = 0; //Mp깎일때 조건사용 ex)스킬 사용시 MP 감소할때
    m_statDirty = true;
}


bool Player::CanTakeAnyContactDamage(int64_t nowMs)
{
    return m_nextContactDamageAllowedMs == -1 || nowMs >= m_nextContactDamageAllowedMs;
}


void Player::OnDamaged(int dmg,int64_t nowMs)
{
    std::lock_guard<std::mutex> lock(m_statMutex);
    int cur_hp = 0; 
    cur_hp = m_stat.GetCurHp();
    cur_hp -= dmg;

    K_LOG_TRACE( "%s 플레이어가 공격당했습니다. 받은 데미지 [%d], 남은 체력 [%d].\n", m_name.c_str(), dmg, cur_hp);

    if(cur_hp <= 0){
        cur_hp = 0;
        Dead();
    }
    m_stat.SetCurHp(cur_hp);

    // 다음 피격 가능 시간 설정
    m_nextContactDamageAllowedMs = nowMs + m_contactDamageCooldownMs;
    m_statDirty = true;
}

void Player::Dead()
{
    m_CurrentState = PlayerState::DEAD;

}

ExpResult Player::AddExp(int64_t exp)
{
    std::lock_guard<std::mutex> lock(m_statMutex);
    ExpResult result = m_stat.AddExp(exp);
    m_statDirty = true;
    return result;
}
