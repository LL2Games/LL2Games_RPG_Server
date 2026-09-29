#include "Monster.h"
#include "Player.h"
#include <math.h>
#include "MapInstance.h"
#include "K_slog.h"
#include "timeUtility.h"
#include "Projectile.h"
#include "ProjectileManager.h"
#include <limits>
#include <stdexcept>


Monster::Monster() : m_deadRequest(false),m_lastAttacker(nullptr)
{

}

int Monster::Init(const MonsterTemplate &monsterTemplate, const MonsterSpawnData &monsterspawnData)
{
	m_monsterId = monsterTemplate.monsterId;
	m_name = monsterTemplate.name;
	m_hp = monsterTemplate.hp;
	m_maxhp = monsterTemplate.hp;
	m_exp = monsterTemplate.exp;
	m_attackDamage = monsterTemplate.attackDamage;
	m_level = monsterTemplate.level;
	m_moveSpeed = monsterTemplate.moveSpeed;
    m_avoidCliff = monsterTemplate.avoidCliff;
    m_canJump = monsterTemplate.canJump;
	m_isAlive = true;
	m_deadRequest = false;

	//스폰된 맵 ID 초기화
	m_mapId = monsterTemplate.mapId;
	m_mapInstance = monsterTemplate.mapInstance;

	m_Pos.xPos = monsterspawnData.spawnPos.xPos;
	m_Pos.yPos = monsterspawnData.spawnPos.yPos;

	m_dir.xPos = 1.0f;
	m_dir.yPos = 0.0f;
	m_rightBound = m_Pos.xPos + 100.0f;
	m_leftBound = m_Pos.xPos - 100.0f;

	m_spawnPos.xPos = monsterspawnData.spawnPos.xPos;
	m_spawnPos.yPos = monsterspawnData.spawnPos.yPos;

	//K_LOG_DEBUG( "[MonsterInit] instanceId=%d monsterId=%d respawnDelayRaw=%d",monsterspawnData.instanceId,
    //monsterspawnData.monsterId,
    //monsterspawnData.respawnDelay);
	m_respawnDelay = std::chrono::seconds(monsterspawnData.respawnDelay);
	m_itemGroup = monsterspawnData.ItemId;

	m_common_drop_Item_GroupId = monsterTemplate.common_drop_group_id;
	m_unique_drop_Item_GroupId = monsterTemplate.unique_drop_group_id;

	m_instanceId = monsterspawnData.instanceId;

	m_state = E_Patrol;

	//막타 플레이어
	m_lastAttacker = nullptr;
	m_lastAttackerId = 0;
	m_lastAttackTime = 0;

	//원거리공격
	m_isRangedAttack = monsterTemplate.isRanged;
	if (m_isRangedAttack)
	{
		m_projectileId = monsterTemplate.projectileData.id;
		m_projectileDamage = monsterTemplate.projectileData.damage;
		m_projectileSpeed = monsterTemplate.projectileData.speed;
		m_ragedAttackRange = monsterTemplate.projectileData.range;
		m_attackCooldown = monsterTemplate.projectileData.coolDown;
	}

#if 0 /*gunoo22 260223 원거리 공격 TestLog*/
	K_LOG_TRACE( "Monster [%s] initialized. Ranged Attack: %s", m_name.c_str(), m_isRangedAttack ? "Yes" : "No");
	K_LOG_TRACE( "Projectile Data - ID: %d, Damage: %f, Speed: %f, Range: %f, Cooldown: %ld", m_projectileId, m_projectileDamage, m_projectileSpeed, m_ragedAttackRange, m_attackCooldown);
#endif

	m_collider.type = monsterTemplate.collisionType;	
	if(monsterTemplate.collisionType == ColliderType::Rect2D)
	{
		m_collider.type = ColliderType::Rect2D;
		m_collider.rect.offset = monsterTemplate.offset;
		m_collider.rect.halfW = monsterTemplate.half.xPos;
		m_collider.rect.halfH = monsterTemplate.half.yPos;
	}
	else if(monsterTemplate.collisionType == ColliderType::Circle2D)
	{
		m_collider.type = ColliderType::Circle2D;
		m_collider.circle.offset = monsterTemplate.offset;
		m_collider.circle.radius = monsterTemplate.radius;
	}


    m_movement.footOffset = m_collider.type == ColliderType::Rect2D
        ? m_collider.rect.offset.yPos + m_collider.rect.halfH
        : m_collider.type == ColliderType::Circle2D
            ? m_collider.circle.offset.yPos + m_collider.circle.radius : 0.0f;
    if (!m_mapInstance) return -1;
    Movement::ResetAtOrigin(m_Pos, m_movement, m_mapInstance->GetPhysicsMap());
    m_movementEpoch = 1;
    m_moveAxis = 0;
    return 1;
}

int Monster::Update(float dt) {
    if (!m_isAlive || !m_mapInstance || !std::isfinite(dt) || dt <= 0) return 0;
    m_moveAxis = 0;
    switch (m_state) {
    case E_Idle: case E_Move: case E_Patrol: UpdatePatrol(dt); break;
    case E_Chase: case E_RangeAttack: UpdateChase(dt); break;
    default: break;
    }
    const auto& map = m_mapInstance->GetPhysicsMap();
    Movement::Input input{m_moveAxis, 0, false};
    const float nextX = m_Pos.xPos + input.horizontal * m_moveSpeed * dt;
    const auto* floor = Movement::FindPlatform(map, m_movement.platformId);
    if (m_movement.mode == Movement::Mode::Grounded && floor && !Movement::Covers(*floor, nextX)) {
        if (m_canJump) {
            input.jump = true; // 첫 정책: 가장자리에서 1회 점프. 길찾기는 별도 기능.
        } else if (m_avoidCliff) {
            input.horizontal = 0;
            m_dir.xPos = -m_dir.xPos;
            if (m_state == E_Chase || m_state == E_RangeAttack) {
                m_lastAttacker = nullptr;
                m_lastAttackerId = 0;
                m_state = E_Patrol;
            }
        }
    }
    const auto result = Movement::Step(m_Pos, m_movement, map, input,
                                       static_cast<float>(m_moveSpeed), dt);
    if (result.fellOut) {
        ResetPhysicsAtSpawn();
    }
    m_movement.facing = m_dir.xPos < 0 ? -1 : 1;
    return 0;
}

//x축만 이동
int Monster::UpdatePatrol(float dt) {
    (void)dt;
    if (m_Pos.xPos >= m_rightBound) m_dir.xPos = -1;
    if (m_Pos.xPos <= m_leftBound) m_dir.xPos = 1;
    m_moveAxis = m_dir.xPos < 0 ? -1 : 1;
    return 0;
}

bool Monster::IsAttackOnCooldown()
{
	auto now = NowMs();
	return !(m_lastAttackTime == 0 || (now - m_lastAttackTime) >= m_attackCooldown);
}

bool Monster::TryRangedAttack(const Vec2& dir)
{
	//쿨다운 체크
	if (IsAttackOnCooldown())
	{
		K_LOG_TRACE( "공격 쿨다운 중입니다.");
		return false;
	}

	//투사체 생성
	auto projectile = std::make_unique<Projectile>(
		m_Pos,
		dir,
		m_projectileSpeed,
		m_ragedAttackRange,
		m_projectileId,
		m_monsterId
	);

	//맵 인스턴스의 투사체 매니저에 투사체 추가
	m_mapInstance->GetProjectileManager().Add(std::move(projectile));

	//마지막 공격 시간 업데이트
	m_lastAttackTime = NowMs();
	K_LOG_TRACE("Projectile dir: (%f, %f)", dir.xPos, dir.yPos);

	m_state = E_RangeAttack; //공격 상태로 전환

	return true;
}

int Monster::UpdateChase(float dt) {
    Player* player = m_lastAttacker;
    if (!player || !player->IsAlive() || !player->GetCurrentMap() ||
        player->GetCurrentMap()->GetMapId() != m_mapId) {
        m_lastAttacker = nullptr;
        m_lastAttackerId = 0;
        m_state = E_Patrol;
        return UpdatePatrol(dt);
    }
    const Vec2 target = player->GetPos();
    const float dx = target.xPos - m_Pos.xPos;
    const float dy = target.yPos - m_Pos.yPos;
    if (std::fabs(dx) > 0.01f) m_dir.xPos = dx < 0 ? -1.0f : 1.0f;
    m_dir.yPos = 0; // 걸음 방향에는 수직 추적을 사용하지 않는다.
    const float distance = std::sqrt(dx*dx + dy*dy);
    if (m_isRangedAttack && distance <= m_ragedAttackRange) {
        const Vec2 aim = distance > 0.01f
            ? Vec2{dx/distance, dy/distance} : Vec2{m_dir.xPos, 0};
        TryRangedAttack(aim);
        return 0;
    }
    if (std::fabs(dx) < 5.0f) return 0;
    m_moveAxis = dx < 0 ? -1 : 1;
    return 0;
}

int Monster::Dead()
{
	if (m_isAlive && !m_deadRequest)
	{
		K_LOG_TRACE( "Is Dead");
		m_deadRequest = true;
		m_deadTime = std::chrono::steady_clock::now();
		m_isAlive = false;
		m_state = E_Die;
        Movement::Reset(m_movement);
        m_moveAxis = 0;
	}

	return 0;
}

bool Monster::CheckRespawnTime(std::chrono::steady_clock::time_point now)
{
	if (m_isAlive)
		return false;

	if (!m_deadRequest)
		return false;

	return now - m_deadTime >= m_respawnDelay;
}

int Monster::Reset()
{
	K_LOG_DEBUG("[MonsterRespawn] monsterId=%d instanceId=%d mapId=%u pos=(%.1f, %.1f)",
		m_monsterId, m_instanceId, m_mapId, m_spawnPos.xPos, m_spawnPos.yPos);
	ResetPhysicsAtSpawn();
	m_hp = m_maxhp;
	m_isAlive = true;
	m_deadRequest = false;
	m_state = E_Idle;
	m_lastAttackTime = 0.0f;
	m_lastAttacker = nullptr;
	m_lastAttackerId = 0;
	return 1;
}

bool Monster::OnDamaged(Player *Attacker, int damage)
{
	K_LOG_TRACE( "AttackerId[%d] damage[%d]", Attacker->GetId(), damage);
	K_LOG_TRACE( "Monster Pointer[%p]", this);

	// 죽은 뒤 / 죽는 중이라면 무시
	if (!m_isAlive || m_deadRequest)
	{
		K_LOG_TRACE( "이미 죽은 몬스터입니다.");
		return false;
	}
	if (damage <= 0)
	{
		K_LOG_TRACE( "damage[%d] <= 0", damage);
		return false;
	}

	m_lastAttackerId = Attacker->GetId();
	m_lastAttacker = Attacker;
	//한대 맞으면 해당 chase 모드로 전환
	m_state = E_Chase;
	K_LOG_TRACE( "몬스터가 플레이어 %s에게 공격당했습니다. 남은 HP: %d", Attacker->GetName().c_str(), m_hp - damage);
	K_LOG_TRACE( "몬스터 상태[%d]", m_state);

	m_hp -= damage;
	K_LOG_TRACE( "m_hp [%d]", m_hp);
	if (m_hp <= 0)
	{
		m_hp = 0;
		Dead();
		K_LOG_TRACE( "몬스터 죽음");
		return true;
	}

	K_LOG_TRACE( "End");
	return false;
}

void Monster::ResetPhysicsAtSpawn()
{
    if (m_movementEpoch == std::numeric_limits<int>::max())
        throw std::overflow_error("monster movement epoch exhausted");
    m_Pos = m_spawnPos;
    Movement::ResetAtOrigin(m_Pos, m_movement, m_mapInstance->GetPhysicsMap());
    m_moveAxis = 0;
    ++m_movementEpoch;
}

void Monster::ClearTarget(int playerId)
{
    if (m_lastAttackerId != playerId) return;
    m_lastAttacker = nullptr;
    m_lastAttackerId = 0;
    if (m_isAlive) m_state = E_Patrol;
}
