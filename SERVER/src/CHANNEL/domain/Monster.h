#pragma once
#include "common.h"
#include "CommonEnum.h"
#include "Collider.h"
//#include "Player.h"

class Player;
class MapInstance;



enum MonsterState {
	E_Idle,
	E_Patrol,
	E_Chase,
	E_Move,
	E_Die,
	E_Hit,
	E_RangeAttack,
	E_Dead,
  E_NONE,
};

struct MonsterHitResult {
    int monster_instance_id;
    int damage;
    int cur_hp;
    int max_hp;
    bool dead;	
};

class Monster
{
public:
    Monster();
    ~Monster(){};
	
	// 몬스터 정보와 스폰 정보 설정 
	int Init(const MonsterTemplate& monsterTemplate, const MonsterSpawnData& monsterspawnData);
	
	// 몬스터 업데이트
	int Update(float dt = 1.0f);
	int UpdatePatrol(float dt= 1.0f); 
	int UpdateChase(float dt= 1.0f);

	// 몬스터 사망
	int Dead();

	// 몬스터가 죽고 리스폰 됐을 때 사용
	int Reset();

	bool OnDamaged(Player* Attacker, int damage);
	
	// 죽고난 후 시간 확인 
	bool CheckRespawnTime(std::chrono::steady_clock::time_point now);
	
public:
	// 몬스터 위치 설정
    void SetPos(Vec2 Pos){this->m_Pos.xPos = Pos.xPos; this->m_Pos.yPos = Pos.yPos;}
	Vec2 GetPos() const {return m_Pos;}

	int GetLevel() const {return m_level;}
	bool IsAlive() {return m_isAlive;}

	bool IsAlive() const {return m_isAlive;}

	int GetLastAttackerID() {return m_lastAttackerId;}
	Player* GetLastAttacker() {return m_lastAttacker;}

	int64_t GetExp(){return m_exp;}

	int GetItemGroup(){return m_itemGroup;}

	std::string& GetCommonItemGroupID(){return m_common_drop_Item_GroupId;}
	std::string& GetUniqueItemGroupID(){return m_unique_drop_Item_GroupId;}
	std::string GetName(){return m_name;}
	int GetInstanceId() const {return m_instanceId;}
	int GetId() const {return m_monsterId;}
	uint32_t GetMapId() const {return m_mapId;}

	int GetCurrentHP() const {return m_hp;}
	int GetMaxHP() const {return m_maxhp;}
	int GetCurrentHP() {return m_hp;}
	int GetMaxHP() {return m_maxhp;}
	Vec2 GetDir() {return m_dir;}
	int GetMoveSpeed() {return m_moveSpeed;}
	MonsterState GetState(){return m_state;}
	void SetState(MonsterState state){m_state =state;}

	int GetDamage(){return m_attackDamage;}
	Collider2D GetCollider() {return m_collider;}

	//원거리공격 관련 메서드
	bool IsRangedAttack() const { return m_isRangedAttack; }
	Vec2 GetProjectilePos() const { return m_projectilePos; }
	float GetProjectileDir() const { return m_projectileDir; }
	bool IsAttackOnCooldown();
	bool TryRangedAttack(const Vec2& dir);
	MonsterState GetState() const { return m_state; }

private:
    MonsterType m_type;
	Collider2D m_collider;

    Vec2 m_Pos;
	Vec2 m_spawnPos;

	float m_rightBound;
	float m_leftBound;
	Vec2  m_dir;

    bool m_isAlive;
	bool m_deadRequest; 
	
	std::string m_name;
    int m_hp;
    int m_maxhp;

    //몬스터 상태 값 나중에 추가해야함
    MonsterState m_state;
	int64_t m_exp;
    int m_attackDamage;
    int m_level;
	int m_moveSpeed;

	// 같은 몬스터라도 구별할 수 있는 고유 값이 필요하기 때문에 추가
	int m_instanceId;
	int m_monsterId;
	
	// 죽었을 때 찍히는 시간
	std::chrono::steady_clock::time_point m_deadTime;
	// 리스폰 시간
	std::chrono::seconds m_respawnDelay;

	// 몬스터 아이템 드롭 그룹
	int m_itemGroup;
	std::string m_common_drop_Item_GroupId;
	std::string m_unique_drop_Item_GroupId;
	
	int m_lastAttackerId;
	Player *m_lastAttacker;

	//스폰된 맵 ID
	uint32_t m_mapId;
	MapInstance* m_mapInstance;

	/***원거리 공격 관련 변수***/
	//고정값
	bool m_isRangedAttack; //원거리 공격 여부
	int m_projectileId; //투사체 아이디
	float m_projectileDamage; //투사체 데미지
	float m_projectileSpeed; //투사체 속도
	float m_ragedAttackRange; //투사체 공격 범위

	//변동값
	Vec2 m_projectilePos; //투사체 위치
	float m_projectileDir; //투사체 방향
	int64_t m_attackCooldown; //공격 쿨타임
	int64_t m_lastAttackTime; //마지막 공격 시간
	/***원거리 공격 관련 변수***/

};