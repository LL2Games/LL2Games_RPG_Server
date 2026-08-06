#include "PlayerStateRepository.h"

#include <cstring>

namespace
{
    class StatementGuard
    {
    public:
        explicit StatementGuard(MYSQL* connection)
            : m_statement(mysql_stmt_init(connection))
        {
        }

        ~StatementGuard()
        {
            if (m_statement != nullptr)
            {
                mysql_stmt_close(m_statement);
            }
        }

        StatementGuard(const StatementGuard&) = delete;
        StatementGuard& operator=(const StatementGuard&) = delete;

        MYSQL_STMT* Get() const
        {
            return m_statement;
        }

        explicit operator bool() const
        {
            return m_statement != nullptr;
        }

    private:
        MYSQL_STMT* m_statement = nullptr;
        };

    class ConnectionGuard
    {
    public:
        explicit ConnectionGuard(MySqlConnectionPool* pool)
            : m_pool(pool),
              m_connection(pool != nullptr? pool->GetConnection()  : nullptr)
        {
        }

        ~ConnectionGuard()
        {
            if (m_pool != nullptr && m_connection != nullptr)
            {
                m_pool->ReleaseConnection(m_connection);
            }
        }

        ConnectionGuard(const ConnectionGuard&) = delete;
        ConnectionGuard& operator=(const ConnectionGuard&) = delete;

        MYSQL* Get() const
        {
            return m_connection;
        }

        explicit operator bool() const
        {
            return m_connection != nullptr;
        }

    private:
        MySqlConnectionPool* m_pool = nullptr;
        MYSQL* m_connection = nullptr;
    };

        bool ExecuteForCharacter(MYSQL* connection, const char* query, const int characterId,std::string& errMsg)
        {
        StatementGuard statement(connection);

        if (!statement)
        {
            errMsg = mysql_error(connection);
            return false;
        }

        if (mysql_stmt_prepare(statement.Get(), query, std::strlen(query)) != 0)
        {
            errMsg = mysql_stmt_error(statement.Get());
            return false;
        }

        int bindCharacterId = characterId;

        MYSQL_BIND parameter[1]{};
        parameter[0].buffer_type = MYSQL_TYPE_LONG;
        parameter[0].buffer = &bindCharacterId;

        if (mysql_stmt_bind_param(statement.Get(), parameter) != 0)
        {
            errMsg = mysql_stmt_error(statement.Get());
            return false;
        }

        if (mysql_stmt_execute(statement.Get()) != 0)
        {
            errMsg = mysql_stmt_error(statement.Get());
            return false;
        }

        return true;
    }
}


PlayerStateRepository::PlayerStateRepository()
    : m_mySql(MySqlConnectionPool::GetInstance())
{
}



bool PlayerStateRepository::SaveCharacter(MYSQL* connection, const PlayerSaveData& saveData, std::string& errMsg)
{
    StatementGuard statement(connection);

    if (!statement)
    {
        errMsg = mysql_error(connection);
        return false;
    }

    const char* query =
        "UPDATE `character` "
        "SET level = ?, map_id = ?, `pos.x` = ?, `pos.y` = ? "
        "WHERE char_id = ?";

    if (mysql_stmt_prepare(
            statement.Get(),
            query,
            std::strlen(query)) != 0)
    {
        errMsg = mysql_stmt_error(statement.Get());
        return false;
    }

    int level = saveData.stat.GetLevel();
    int mapId = saveData.mapId;
    float xPosition = saveData.position.xPos;
    float yPosition = saveData.position.yPos;
    int characterId = saveData.characterId;

    MYSQL_BIND parameters[5]{};

    parameters[0].buffer_type = MYSQL_TYPE_LONG;
    parameters[0].buffer = &level;

    parameters[1].buffer_type = MYSQL_TYPE_LONG;
    parameters[1].buffer = &mapId;

    parameters[2].buffer_type = MYSQL_TYPE_FLOAT;
    parameters[2].buffer = &xPosition;

    parameters[3].buffer_type = MYSQL_TYPE_FLOAT;
    parameters[3].buffer = &yPosition;

    parameters[4].buffer_type = MYSQL_TYPE_LONG;
    parameters[4].buffer = &characterId;

    if (mysql_stmt_bind_param(statement.Get(), parameters) != 0)
    {
        errMsg = mysql_stmt_error(statement.Get());
        return false;
    }

    if (mysql_stmt_execute(statement.Get()) != 0)
    {
        errMsg = mysql_stmt_error(statement.Get());
        return false;
    }

    return true;
}

bool PlayerStateRepository::SaveCharacterStat(MYSQL* connection, const PlayerSaveData& saveData, std::string& errMsg)
{
    StatementGuard statement(connection);

    if (!statement)
    {
        errMsg = mysql_error(connection);
        return false;
    }

    const char* query =
        "UPDATE character_stat "
        "SET str = ?, dex = ?, intel = ?, luk = ?, "
        "max_hp = ?, max_mp = ?, cur_hp = ?, cur_mp = ?, "
        "remain_ap = ?, exp = ?, level = ? "
        "WHERE char_id = ?";

    if (mysql_stmt_prepare(statement.Get(), query, std::strlen(query)) != 0)
    {
        errMsg = mysql_stmt_error(statement.Get());
        return false;
    }

    const BaseStat baseStat = saveData.stat.GetBase();

    int strength = baseStat.str;
    int dexterity = baseStat.dex;
    int intelligence = baseStat.intel;
    int luck = baseStat.luck;

    int maxHp = saveData.stat.GetMaxHp();
    int maxMp = saveData.stat.GetMaxMp();
    int currentHp = saveData.stat.GetCurHp();
    int currentMp = saveData.stat.GetCurMp();
    int remainAp = saveData.stat.GetRemainAp();

    long long experience =
        static_cast<long long>(saveData.stat.GetExp());

    int level = saveData.stat.GetLevel();
    int characterId = saveData.characterId;

    MYSQL_BIND parameters[12]{};

    parameters[0].buffer_type = MYSQL_TYPE_LONG;
    parameters[0].buffer = &strength;

    parameters[1].buffer_type = MYSQL_TYPE_LONG;
    parameters[1].buffer = &dexterity;

    parameters[2].buffer_type = MYSQL_TYPE_LONG;
    parameters[2].buffer = &intelligence;

    parameters[3].buffer_type = MYSQL_TYPE_LONG;
    parameters[3].buffer = &luck;

    parameters[4].buffer_type = MYSQL_TYPE_LONG;
    parameters[4].buffer = &maxHp;

    parameters[5].buffer_type = MYSQL_TYPE_LONG;
    parameters[5].buffer = &maxMp;

    parameters[6].buffer_type = MYSQL_TYPE_LONG;
    parameters[6].buffer = &currentHp;

    parameters[7].buffer_type = MYSQL_TYPE_LONG;
    parameters[7].buffer = &currentMp;

    parameters[8].buffer_type = MYSQL_TYPE_LONG;
    parameters[8].buffer = &remainAp;

    parameters[9].buffer_type = MYSQL_TYPE_LONGLONG;
    parameters[9].buffer = &experience;

    parameters[10].buffer_type = MYSQL_TYPE_LONG;
    parameters[10].buffer = &level;

    parameters[11].buffer_type = MYSQL_TYPE_LONG;
    parameters[11].buffer = &characterId;

    if (mysql_stmt_bind_param(statement.Get(), parameters) != 0)
    {
        errMsg = mysql_stmt_error(statement.Get());
        return false;
    }

    if (mysql_stmt_execute(statement.Get()) != 0)
    {
        errMsg = mysql_stmt_error(statement.Get());
        return false;
    }

    return true;
}


bool PlayerStateRepository::SaveInventory(
    MYSQL* connection,
    const PlayerSaveData& saveData,
    std::string& errMsg)
{
    // FK가 있다면 아이템을 메타보다 먼저 삭제해야 한다.
    if (!ExecuteForCharacter(
            connection,
            "DELETE FROM character_inventory WHERE char_id = ?",
            saveData.characterId,
            errMsg))
    {
        return false;
    }

    if (!ExecuteForCharacter(
            connection,
            "DELETE FROM character_inventory_meta WHERE char_id = ?",
            saveData.characterId,
            errMsg))
    {
        return false;
    }

    // 인벤토리 메타 저장
    {
        StatementGuard statement(connection);

        if (!statement)
        {
            errMsg = mysql_error(connection);
            return false;
        }

        const char* query =
            "INSERT INTO character_inventory_meta "
            "(char_id, inventory_type, max_slot, current_slot_count) "
            "VALUES (?, ?, ?, ?)";

        if (mysql_stmt_prepare(statement.Get(), query, std::strlen(query)) != 0)
        {
            errMsg = mysql_stmt_error(statement.Get());
            return false;
        }

        int characterId = saveData.characterId;
        int inventoryType = 0;
        int maxSlot = 0;
        int currentSlotCount = 0;

        MYSQL_BIND parameters[4]{};

        parameters[0].buffer_type = MYSQL_TYPE_LONG;
        parameters[0].buffer = &characterId;

        parameters[1].buffer_type = MYSQL_TYPE_LONG;
        parameters[1].buffer = &inventoryType;

        parameters[2].buffer_type = MYSQL_TYPE_LONG;
        parameters[2].buffer = &maxSlot;

        parameters[3].buffer_type = MYSQL_TYPE_LONG;
        parameters[3].buffer = &currentSlotCount;

        if (mysql_stmt_bind_param(statement.Get(), parameters) != 0)
        {
            errMsg = mysql_stmt_error(statement.Get());
            return false;
        }

        for (const InventoryMetaInfo& metaInfo : saveData.inventoryMetas)
        {
            inventoryType = metaInfo.inventoryType;
            maxSlot = metaInfo.max_slots;
            currentSlotCount = metaInfo.currnet_slots_size;

            if (mysql_stmt_execute(statement.Get()) != 0)
            {
                errMsg = mysql_stmt_error(statement.Get());
                return false;
            }
        }
    }

    // 인벤토리 아이템 저장
    {
        StatementGuard statement(connection);

        if (!statement)
        {
            errMsg = mysql_error(connection);
            return false;
        }

        const char* query =
            "INSERT INTO character_inventory "
            "(char_id, inventory_type, slot_pos, item_Id, item_count) "
            "VALUES (?, ?, ?, ?, ?)";

        if (mysql_stmt_prepare(statement.Get(),query,std::strlen(query)) != 0)
        {
            errMsg = mysql_stmt_error(statement.Get());
            return false;
        }

        int characterId = saveData.characterId;
        int inventoryType = 0;
        int slotPosition = 0;
        int itemId = 0;
        int itemCount = 0;

        MYSQL_BIND parameters[5]{};

        parameters[0].buffer_type = MYSQL_TYPE_LONG;
        parameters[0].buffer = &characterId;

        parameters[1].buffer_type = MYSQL_TYPE_LONG;
        parameters[1].buffer = &inventoryType;

        parameters[2].buffer_type = MYSQL_TYPE_LONG;
        parameters[2].buffer = &slotPosition;

        parameters[3].buffer_type = MYSQL_TYPE_LONG;
        parameters[3].buffer = &itemId;

        parameters[4].buffer_type = MYSQL_TYPE_LONG;
        parameters[4].buffer = &itemCount;

        if (mysql_stmt_bind_param(statement.Get(), parameters) != 0)
        {
            errMsg = mysql_stmt_error(statement.Get());
            return false;
        }

        for (const InventoryItemInfo& itemInfo : saveData.inventoryItems)
        {
            inventoryType = itemInfo.inventoryType;
            slotPosition = itemInfo.slotPos;
            itemId = itemInfo.itemId;
            itemCount = itemInfo.itemCount;

            if (mysql_stmt_execute(statement.Get()) != 0)
            {
                errMsg = mysql_stmt_error(statement.Get());
                return false;
            }
        }
    }

    return true;
}

bool PlayerStateRepository::Save(const PlayerSaveData& saveData, std::string& errMsg)
{
    errMsg.clear();

    if (m_mySql == nullptr)
    {
        errMsg = "MySQL connection pool is not initialized";
        return false;
    }

    ConnectionGuard connectionGuard(m_mySql);

    if (!connectionGuard)
    {
        errMsg = "Failed to acquire MySQL connection";
        return false;
    }

    MYSQL* connection = connectionGuard.Get();

    if (mysql_query(connection, "START TRANSACTION") != 0)
    {
        errMsg = mysql_error(connection);
        return false;
    }

    const bool saveSucceeded =
        SaveCharacter(connection, saveData, errMsg) &&
        SaveCharacterStat(connection, saveData, errMsg) &&
        SaveInventory(connection, saveData, errMsg) &&
        SaveQuickSlots(connection, saveData, errMsg);

    if (!saveSucceeded)
    {
        const std::string saveError = errMsg;

        if (mysql_rollback(connection) != 0)
        {
            errMsg =saveError + "; rollback failed: " + mysql_error(connection);
        }

        return false;
    }

    if (mysql_commit(connection) != 0)
    {
        const std::string commitError = mysql_error(connection);

        if (mysql_rollback(connection) != 0)
        {
            errMsg =commitError +"; rollback failed: " + mysql_error(connection);
        }
        else
        {
            errMsg = commitError;
        }

        return false;
    }

    return true;
}


bool PlayerStateRepository::SaveQuickSlots(MYSQL* connection, const PlayerSaveData& saveData, std::string& errMsg)
{
    if (!ExecuteForCharacter(
            connection,
            "DELETE FROM character_quickslot WHERE char_id = ?",
            saveData.characterId,
            errMsg))
    {
        return false;
    }

    StatementGuard statement(connection);

    if (!statement)
    {
        errMsg = mysql_error(connection);
        return false;
    }

    const char* query =
        "INSERT INTO character_quickslot "
        "(char_id, slot_index, quickslot_type, skill_id, "
        "inventory_type, inventory_slot_pos) "
        "VALUES (?, ?, ?, ?, ?, ?)";

    if (mysql_stmt_prepare(statement.Get(), query, std::strlen(query)) != 0)
    {
        errMsg = mysql_stmt_error(statement.Get());
        return false;
    }

    int characterId = saveData.characterId;
    int slotIndex = 0;
    int slotType = 0;
    int referenceId = 0;
    int inventoryType = 0;
    int inventorySlotPosition = 0;

    MYSQL_BIND parameters[6]{};

    parameters[0].buffer_type = MYSQL_TYPE_LONG;
    parameters[0].buffer = &characterId;

    parameters[1].buffer_type = MYSQL_TYPE_LONG;
    parameters[1].buffer = &slotIndex;

    parameters[2].buffer_type = MYSQL_TYPE_LONG;
    parameters[2].buffer = &slotType;

    parameters[3].buffer_type = MYSQL_TYPE_LONG;
    parameters[3].buffer = &referenceId;

    parameters[4].buffer_type = MYSQL_TYPE_LONG;
    parameters[4].buffer = &inventoryType;

    parameters[5].buffer_type = MYSQL_TYPE_LONG;
    parameters[5].buffer = &inventorySlotPosition;

    if (mysql_stmt_bind_param(statement.Get(), parameters) != 0)
    {
        errMsg = mysql_stmt_error(statement.Get());
        return false;
    }

    for (const QuickSlotData& quickSlot : saveData.quickSlots)
    {
        if (quickSlot.type == QuickSlotType::None)
        {
            continue;
        }

        slotIndex = quickSlot.slot_index;
        slotType = static_cast<int>(quickSlot.type);
        referenceId = quickSlot.ref_id;
        inventoryType = quickSlot.inventory_type;
        inventorySlotPosition = quickSlot.inventory_slotPos;

        if (mysql_stmt_execute(statement.Get()) != 0)
        {
            errMsg = mysql_stmt_error(statement.Get());
            return false;
        }
    }

    return true;
}