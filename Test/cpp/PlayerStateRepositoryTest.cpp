#include "MySqlConnectionPool.h"
#include "PlayerStateRepository.h"

#include <cstdlib>
#include <iostream>
#include <string>

constexpr int kTestCharacterId = 900001;

namespace
{
    bool Check(const bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "[FAIL] " << message << '\n';
            return false;
        }
        std::cout << "[PASS] " << message << '\n';
        return true;
    }

    bool ReadRequiredEnvironment(const char* name, std::string& value)
    {
        const char* environmentValue = std::getenv(name);

        if (environmentValue == nullptr || environmentValue[0] == '\0')
        {
            std::cerr << "[FAIL] " << name<< " is not configured\n";
            return false;
        }

        value = environmentValue;
        return true;
    }

    bool ExecuteSql(MYSQL* connection,const char* query,const char* description)
    {
        if (mysql_query(connection, query) != 0)
        {
            std::cerr << "[FAIL] " << description << ": " << mysql_error(connection)<< '\n';
            return false;
        }

        return true;
    }

bool CreateTemporaryTables(MYSQL* connection)
{
    return
        ExecuteSql(
            connection,
            "CREATE TEMPORARY TABLE `character` ("
            "char_id INT NOT NULL PRIMARY KEY, "
            "level INT NOT NULL, "
            "map_id INT NOT NULL, "
            "`pos.x` FLOAT NOT NULL, "
            "`pos.y` FLOAT NOT NULL"
            ") ENGINE=InnoDB",
            "create temporary character table") &&

        ExecuteSql(
            connection,
            "CREATE TEMPORARY TABLE character_stat ("
            "char_id INT NOT NULL PRIMARY KEY, "
            "str INT NOT NULL, "
            "dex INT NOT NULL, "
            "intel INT NOT NULL, "
            "luk INT NOT NULL, "
            "max_hp INT NOT NULL, "
            "max_mp INT NOT NULL, "
            "cur_hp INT NOT NULL, "
            "cur_mp INT NOT NULL, "
            "remain_ap INT NOT NULL, "
            "exp BIGINT NOT NULL, "
            "level INT NOT NULL"
            ") ENGINE=InnoDB",
            "create temporary character_stat table") &&

        ExecuteSql(
            connection,
            "CREATE TEMPORARY TABLE character_inventory_meta ("
            "char_id INT NOT NULL, "
            "inventory_type INT NOT NULL, "
            "max_slot INT NOT NULL, "
            "current_slot_count INT NOT NULL, "
            "PRIMARY KEY (char_id, inventory_type)"
            ") ENGINE=InnoDB",
            "create temporary inventory meta table") &&

        ExecuteSql(
            connection,
            "CREATE TEMPORARY TABLE character_inventory ("
            "char_id INT NOT NULL, "
            "inventory_type INT NOT NULL, "
            "slot_pos INT NOT NULL, "
            "item_Id INT NOT NULL, "
            "item_count INT NOT NULL, "
            "PRIMARY KEY (char_id, inventory_type, slot_pos)"
            ") ENGINE=InnoDB",
            "create temporary inventory table") &&

        ExecuteSql(
            connection,
            "CREATE TEMPORARY TABLE character_quickslot ("
            "char_id INT NOT NULL, "
            "slot_index INT NOT NULL, "
            "quickslot_type INT NOT NULL, "
            "skill_id INT NOT NULL, "
            "inventory_type INT NOT NULL, "
            "inventory_slot_pos INT NOT NULL, "
            "PRIMARY KEY (char_id, slot_index)"
            ") ENGINE=InnoDB",
            "create temporary quickslot table");
}

    bool InsertInitialData(MYSQL* connection)
{
    return
        ExecuteSql(
            connection,
            "INSERT INTO `character` "
            "(char_id, level, map_id, `pos.x`, `pos.y`) "
            "VALUES (900001, 1, 100000000, 10.0, 20.0)",
            "insert initial character") &&

        ExecuteSql(
            connection,
            "INSERT INTO character_stat "
            "(char_id, str, dex, intel, luk, "
            "max_hp, max_mp, cur_hp, cur_mp, "
            "remain_ap, exp, level) "
            "VALUES "
            "(900001, 4, 4, 4, 4, "
            "100, 50, 100, 50, "
            "0, 0, 1)",
            "insert initial character stat") &&

        ExecuteSql(
            connection,
            "INSERT INTO character_inventory_meta "
            "(char_id, inventory_type, max_slot, current_slot_count) "
            "VALUES (900001, 0, 10, 1)",
            "insert initial inventory meta") &&

        ExecuteSql(
            connection,
            "INSERT INTO character_inventory "
            "(char_id, inventory_type, slot_pos, item_Id, item_count) "
            "VALUES (900001, 0, 0, 1000, 1)",
            "insert initial inventory item") &&

        ExecuteSql(
            connection,
            "INSERT INTO character_quickslot "
            "(char_id, slot_index, quickslot_type, skill_id, "
            "inventory_type, inventory_slot_pos) "
            "VALUES (900001, 0, 1, 1000, 0, 0)",
            "insert initial quickslot");
}

PlayerSaveData MakeTestSaveData()
{
    return PlayerSaveData{1,kTestCharacterId,100000001, Vec2{123.5F, 456.25F},

        CharacterStat{
            BaseStat{31, 22, 13, 14},
            DerivedStat{950, 420},
            ExpStat{17, 7654321, 9000000},
            875,
            315,
            6
        },

        {
            InventoryMetaInfo{0, 24, 1},
            InventoryMetaInfo{1, 32, 1}
        },

        {
            InventoryItemInfo{0, 3, 1001001, 1},
            InventoryItemInfo{1, 5, 2000001, 37}
        },

        {
            QuickSlotData{
                0,
                QuickSlotType::Skill,
                1001,
                0,
                0,
                0
            },
            QuickSlotData{
                1,
                QuickSlotType::Item,
                2000001,
                1,
                5,
                37
            }
        }
    };
}

bool CheckRowCount(
    MYSQL* connection,
    const char* query,
    const unsigned long long expectedCount,
    const char* message)
{
    if (mysql_query(connection, query) != 0)
    {
        std::cerr
            << "[FAIL] " << message
            << ": " << mysql_error(connection)
            << '\n';

        return false;
    }

    MYSQL_RES* result = mysql_store_result(connection);

    if (result == nullptr)
    {
        std::cerr
            << "[FAIL] " << message
            << ": result retrieval failed: "
            << mysql_error(connection)
            << '\n';

        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(result);

    const bool succeeded =
        row != nullptr &&
        row[0] != nullptr &&
        std::strtoull(row[0], nullptr, 10) == expectedCount;

    mysql_free_result(result);

    return Check(succeeded, message);
}

bool VerifySavedState(MYSQL* connection)
{
    return
        CheckRowCount(
            connection,
            "SELECT COUNT(*) FROM `character` "
            "WHERE char_id = 900001 "
            "AND level = 17 "
            "AND map_id = 100000001 "
            "AND ABS(`pos.x` - 123.5) < 0.001 "
            "AND ABS(`pos.y` - 456.25) < 0.001",
            1,
            "캐릭터 맵·좌표·레벨 저장 확인") &&

        CheckRowCount(
            connection,
            "SELECT COUNT(*) FROM character_stat "
            "WHERE char_id = 900001 "
            "AND str = 31 "
            "AND dex = 22 "
            "AND intel = 13 "
            "AND luk = 14 "
            "AND max_hp = 950 "
            "AND max_mp = 420 "
            "AND cur_hp = 875 "
            "AND cur_mp = 315 "
            "AND remain_ap = 6 "
            "AND exp = 7654321 "
            "AND level = 17",
            1,
            "캐릭터 스탯 저장 확인") &&

        CheckRowCount(
            connection,
            "SELECT COUNT(*) FROM character_inventory_meta "
            "WHERE char_id = 900001 "
            "AND ("
            "(inventory_type = 0 AND max_slot = 24 "
            "AND current_slot_count = 1) "
            "OR "
            "(inventory_type = 1 AND max_slot = 32 "
            "AND current_slot_count = 1)"
            ")",
            2,
            "인벤토리 메타데이터 교체 확인") &&

        CheckRowCount(
            connection,
            "SELECT COUNT(*) FROM character_inventory "
            "WHERE char_id = 900001 "
            "AND ("
            "(inventory_type = 0 AND slot_pos = 3 "
            "AND item_Id = 1001001 AND item_count = 1) "
            "OR "
            "(inventory_type = 1 AND slot_pos = 5 "
            "AND item_Id = 2000001 AND item_count = 37)"
            ")",
            2,
            "인벤토리 아이템 교체 확인") &&

        CheckRowCount(
            connection,
            "SELECT COUNT(*) FROM character_quickslot "
            "WHERE char_id = 900001 "
            "AND ("
            "(slot_index = 0 AND quickslot_type = 1 "
            "AND skill_id = 1001 "
            "AND inventory_type = 0 "
            "AND inventory_slot_pos = 0) "
            "OR "
            "(slot_index = 1 AND quickslot_type = 2 "
            "AND skill_id = 2000001 "
            "AND inventory_type = 1 "
            "AND inventory_slot_pos = 5)"
            ")",
            2,
            "퀵슬롯 교체 확인");
}

PlayerSaveData MakeRollbackFailureData()
{
    return PlayerSaveData{
        2,
        kTestCharacterId,
        999999999,
        Vec2{-500.0F, -600.0F},

        CharacterStat{BaseStat{99, 98, 97, 96},DerivedStat{9999, 8888},ExpStat{99, 99999999, 100000000},7777,6666,55},
        {
            InventoryMetaInfo{4, 99, 1}
        },

        {
            InventoryItemInfo{4, 9, 9999999, 99}
        },

        {
            // 동일한 slot_index를 두 번 넣어 PK 중복을 발생시킨다.
            QuickSlotData{7, QuickSlotType::Skill,3001,0,0,0},
            QuickSlotData{7,QuickSlotType::Item,4001,1,2,1}
        }
    };
}

}

int main()
{
    MySqlConfig mysqlConfig{};

    if (!ReadRequiredEnvironment("TEST_MYSQL_HOST", mysqlConfig.host) ||
        !ReadRequiredEnvironment("TEST_MYSQL_USER", mysqlConfig.user) ||
        !ReadRequiredEnvironment("TEST_MYSQL_PASSWORD", mysqlConfig.password) ||
        !ReadRequiredEnvironment("TEST_MYSQL_DATABASE", mysqlConfig.database))
    {
        return EXIT_FAILURE;
    }

    mysqlConfig.port = 3306;
    mysqlConfig.poolCount = 1;

    const char* portText = std::getenv("TEST_MYSQL_PORT");

    if (portText != nullptr)
    {
        try
        {
            mysqlConfig.port = std::stoi(portText);
        }
        catch (const std::exception&)
        {
            std::cerr << "[FAIL] TEST_MYSQL_PORT is invalid\n";
            return EXIT_FAILURE;
        }
    }

    if (mysqlConfig.port <= 0 || mysqlConfig.port > 65535)
    {
        std::cerr << "[FAIL] MySQL port is out of range\n";
        return EXIT_FAILURE;
    }

    if (MySqlConnectionPool::Init(mysqlConfig, 1) != 0)
    {
        std::cerr << "[FAIL] MySQL connection pool initialization failed\n";

        return EXIT_FAILURE;
    }

    MySqlConnectionPool* pool = MySqlConnectionPool::GetInstance();

    if (!Check(pool != nullptr, "MySQL 연결 풀 초기화 성공"))
    {
        return EXIT_FAILURE;
    }

    MYSQL* connection = pool->GetConnection();

    if (!Check(connection != nullptr, "테스트 MySQL 연결 성공"))
    {
        return EXIT_FAILURE;
    }

    if (!Check(CreateTemporaryTables(connection),"임시 저장 테이블 생성 성공"))
    {
        pool->ReleaseConnection(connection);
        return EXIT_FAILURE;
    }

    if (!Check(InsertInitialData(connection),"초기 플레이어 상태 등록 성공"))
    {
        pool->ReleaseConnection(connection);
        return EXIT_FAILURE;
    }

    pool->ReleaseConnection(connection);

    const PlayerSaveData saveData = MakeTestSaveData();

    PlayerStateRepository repository;
    std::string saveError;

    const bool saveSucceeded = repository.Save(saveData, saveError);

    if (!saveSucceeded)
    {
        std::cerr << "[DETAIL] Player state save error: " << saveError<< '\n';
    }

    if (!Check(saveSucceeded,"플레이어 상태 트랜잭션 커밋 성공"))
    {
        return EXIT_FAILURE;
    }

    connection = pool->GetConnection();

    if (!Check(connection != nullptr,"저장 상태 검증을 위한 MySQL 재연결 성공"))
    {
        return EXIT_FAILURE;
    }

    const bool savedStateVerified =VerifySavedState(connection);

    pool->ReleaseConnection(connection);

    if (!savedStateVerified)
    {
        return EXIT_FAILURE;
    }


    // 2. 고의로 중복 퀵슬롯 저장 시도
    const PlayerSaveData rollbackFailureData = MakeRollbackFailureData();

    std::string rollbackError;

    const bool invalidSaveSucceeded =repository.Save(rollbackFailureData,rollbackError);

    if (!invalidSaveSucceeded)
    {
        std::cout << "[INFO] 예상된 저장 실패: "<< rollbackError<< '\n';
    }

    if (!Check(!invalidSaveSucceeded,"중복 퀵슬롯 트랜잭션 거부"))
    {
        return EXIT_FAILURE;
    }

    if (!Check(!rollbackError.empty(),"저장소의 트랜잭션 실패 사유 반환 확인"))
    {
        return EXIT_FAILURE;
    }


    // 3. 실패 전 정상 데이터가 유지됐는지 확인
    connection = pool->GetConnection();

    if (!Check(connection != nullptr, "롤백 검증을 위한 MySQL 재연결 성공"))
    {
        return EXIT_FAILURE;
    }

    const bool rollbackPreservedState =VerifySavedState(connection);

    pool->ReleaseConnection(connection);

    if (!Check(rollbackPreservedState,"롤백 후 이전 커밋 상태 보존 확인"))
    {
        return EXIT_FAILURE;
    }

    std::cout << "플레이어 상태 저장소 통합 테스트 전체 통과\n";

    return EXIT_SUCCESS;
}

