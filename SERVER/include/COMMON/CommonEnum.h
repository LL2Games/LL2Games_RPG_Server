#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <optional>
#include "Collider.h"


class MapInstance;
class Player;
#pragma once
enum class MapRegion{
    HENESYS,
    ELLINIA,
    PERION,
    KERNING_CITY
};

enum class MonsterType {
    SNAIL,
    ORANGE_MUSHROOM,
    SLIME,
    STUMP,
    PIG
};

// Map Json 파일에서 읽어온 몬스터 정보를 저장하는 구조체
struct MonsterSpawnData {
    MonsterType type;
    Vec2 spawnPos;
     int respawnDelay;
    int monsterId;
    int instanceId;
    int ItemId;
};

struct MonsterProjectileData {
    int id;
    float damage;
    float speed;
    float range;
    int64_t coolDown;
    
    Collider2D collider;
};

// 몬스터 Json 파일에서 읽어온 값을 저장하는 구조체
struct MonsterTemplate {
    int monsterId;
    int level;
    int hp;
    int attackDamage;
    int64_t exp;
    float moveSpeed;
    std::string name;
    std::string common_drop_group_id;
    std::string unique_drop_group_id;

    ColliderType collisionType;
    Vec2 offset;
    Vec2 half;
    float radius;

    // 몬스터와 플레이어간 접촉 여부 계산을 판단하는 거리 값 - 이 거리 안에 들어가면 접촉 했는지 안 했는지 계산
    float broadCutSq = 0.f;

    uint16_t mapId;
    MapInstance* mapInstance;

    //투사체 정보
    bool isRanged;
    MonsterProjectileData projectileData;
};

struct MonsterMoveInfo
{
    int instanceId;
    int state;
    int dirX;
    float xPos;
    float yPos;
    int currentHp;
    int maxHp;
};

struct MonsterSnapshotInfo
{
    int monsterId;
    int instanceId;
    int dirX;
    int currentHp;
    int maxHp;
    int state;
    int moveSpeed;
    float xPos;
    float yPos;
    
};

struct MonsterRespawnInfo
{
	int instanceId = 0;
	int monsterId = 0;
	int currentHp = 0;
	int MaxHp = 0;
	int state = 0;
	int dirX = 0;
	float xPos =0.0f;
	float yPos = 0.0f;
		
};


typedef struct MapInitData{
    uint32_t mapID;
	std::string name;
   
    std::vector<MonsterSpawnData> MonstersData;

}mapData;




