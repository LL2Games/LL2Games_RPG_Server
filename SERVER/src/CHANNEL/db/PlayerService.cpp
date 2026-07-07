#include "PlayerService.h"
#include "RedisUtility.h"
#include "CharacterStat.h"
#include "InventoryManager.h"
#include "QuickSlotManager.h"
#include "LevelManager.h"


#include "K_slog.h"
#include <cstdio>
#include <sstream>
#include <string>


MySqlConnectionPool *PlayerService::m_mySql = nullptr;

PlayerService::PlayerService()
{
    m_mySql = MySqlConnectionPool::GetInstance();
}

PlayerService::~PlayerService()
{
}


std::unique_ptr<Player> PlayerService::LoadPlayer(int characterId, RedisClient* redis)
{
    std::unique_ptr<Player> player = nullptr;
    PlayerInitData playerInit{};
    AllStat allStat{};
    std::map<std::string, std::string> redis_map;
    int result = 0;

    const std::string redisKey = "characterID:" + std::to_string(characterId);

    if (redis != nullptr)
    {
        auto redis_value = redis->HGetAll(redisKey);
        if (redis_value.has_value() && !redis_value->empty())
        {
            player = std::make_unique<Player>();
            GetInt(*redis_value, "char_id", playerInit.char_id);
            GetStr(*redis_value, "name", playerInit.name);
            GetStr(*redis_value, "account_id", playerInit.account_id);
            GetInt(*redis_value, "level", playerInit.level);
            GetInt(*redis_value, "job", playerInit.job);
            GetInt(*redis_value, "root_job", playerInit.root_job);
            GetInt(*redis_value, "map_id", playerInit.map_id);
            GetFloat(*redis_value, "xPos", playerInit.xPos);
            GetFloat(*redis_value, "yPos", playerInit.yPos);

            if (playerInit.char_id == 0)
            {
                playerInit.char_id = characterId;
            }

            BaseStat base{};
            DerivedStat derived{};
            ExpStat expStat{};
            int remainAp;
            int curHp;
            int curMp;
            
            GetInt(*redis_value, "base_str", base.str);
            GetInt(*redis_value, "base_dex", base.dex);
            GetInt(*redis_value, "base_intel", base.intel);
            GetInt(*redis_value, "base_luck", base.luck);
            GetInt(*redis_value, "derived_maxHp", derived.maxHp);
            GetInt(*redis_value, "derived_maxMp", derived.maxMp);
            GetInt(*redis_value, "curHp", curHp);
            GetInt(*redis_value, "curMp", curMp);
            GetInt(*redis_value, "remainAp", remainAp);
            GetInt(*redis_value, "level", expStat.level);
            GetInt64(*redis_value, "exp", expStat.exp);
            GetInt64(*redis_value, "need_exp", expStat.need_exp);

            CharacterStat stat(base, derived, expStat, curHp, curMp, remainAp);
            player->SetInitData(playerInit, stat);

            K_slog_trace(K_SLOG_TRACE, "[Redis]LoadPlayer_stat SUCCESS [%d]", player->GetId());
            return player;
        }
    }

    if(!LoadPlayerInfo(characterId, playerInit))
    {
        K_slog_trace(K_SLOG_ERROR, "LoadPlayerInfo failed [%d]", characterId);
        return nullptr;
    }


    if(!LoadPlayerStat(characterId, allStat))
    {
        K_slog_trace(K_SLOG_ERROR, "LoadPlayerInfo failed [%d]", characterId);
        return nullptr;
    }
    CharacterStat stat(allStat.baseStat, allStat.derivedStat, allStat.expStat, allStat.curHp, allStat.curMp, allStat.remainAp);
    player = std::make_unique<Player>();
    player->SetInitData(playerInit, stat);
    redis_map = PlayerInfoToRedisMap(playerInit, stat);

    if (redis == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "redis is nullptr");
        return nullptr;
    }

    result = redis->HSetAll(redisKey, redis_map, 600);

    K_slog_trace(K_SLOG_TRACE, "HSetAll SUCCESS");

    if (result != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "HSetAll ERROR");
        return nullptr;
    }
    return player;
}

bool PlayerService::LoadInventoryMeta(Player* player)
{
    InventoryMetaInfo inventoryMetaInfo{};
    auto InventoryManager = player->GetInventoryManager();

    if(InventoryManager == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "InventoryManager is nullptr");
        return false;
    }


    MYSQL* conn = m_mySql->GetConnection();
    if(!conn) return false;

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_init ERROR [%s]", mysql_error(conn));
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    const char* query =
        "SELECT inventory_type, max_slot, current_slot_count FROM character_inventory_meta WHERE char_id = ?";

    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_prepare ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    MYSQL_BIND param[1]{};
    int characterId = player->GetId();

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &characterId;

    if (mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_bind_param ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_execute ERROR [%s]", mysql_stmt_error(stmt));
        K_slog_trace(K_SLOG_ERROR, "SQL [%s]", query);

        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    MYSQL_BIND resultBind[3]{};

    resultBind[0].buffer_type = MYSQL_TYPE_LONG;
    resultBind[0].buffer = &inventoryMetaInfo.inventoryType;

    resultBind[1].buffer_type = MYSQL_TYPE_LONG;
    resultBind[1].buffer = &inventoryMetaInfo.max_slots;

    resultBind[2].buffer_type = MYSQL_TYPE_LONG;
    resultBind[2].buffer = &inventoryMetaInfo.currnet_slots_size;
  
    if (mysql_stmt_bind_result(stmt, resultBind) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_bind_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_store_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    while (true)
    {
        int fetchResult = mysql_stmt_fetch(stmt);

        if (fetchResult == MYSQL_NO_DATA)
            break;

        if (fetchResult != 0)
        {
            K_slog_trace(K_SLOG_ERROR, "mysql_stmt_fetch ERROR [%s]", mysql_stmt_error(stmt));

            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            m_mySql->ReleaseConnection(conn);
            return false;
        }

        InventoryManager->CreateInventory(inventoryMetaInfo);
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    m_mySql->ReleaseConnection(conn);
    
    return true;
}

bool PlayerService::LoadInventory(Player* player)
{
    int inventoryType = 0;
    int slotpos = 0;
    int itemId = 0;
    int itemCount = 0;

    auto inventoryManager = player->GetInventoryManager();
    if(inventoryManager == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "inventoryManager is nullptr");
        return false;
    }

    MYSQL* conn = m_mySql->GetConnection(); 
    if(!conn) return false;

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_init ERROR [%s]", mysql_error(conn));
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    const char* query = "SELECT inventory_type, slot_pos, item_Id, item_count FROM character_inventory WHERE char_id = ?";
    
    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_prepare ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    MYSQL_BIND param[1]{};
    int characterId = player->GetId();
    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &characterId;

    if (mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_bind_param ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_execute ERROR [%s]", mysql_stmt_error(stmt));
        K_slog_trace(K_SLOG_ERROR, "SQL [%s]", query);

        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    MYSQL_BIND resultBind[4]{};

    resultBind[0].buffer_type = MYSQL_TYPE_TINY;
    resultBind[0].buffer = &inventoryType;

    resultBind[1].buffer_type = MYSQL_TYPE_LONG;
    resultBind[1].buffer = &slotpos;

    resultBind[2].buffer_type = MYSQL_TYPE_LONG;
    resultBind[2].buffer = &itemId;

    resultBind[3].buffer_type = MYSQL_TYPE_LONG;
    resultBind[3].buffer = &itemCount;
  
    if (mysql_stmt_bind_result(stmt, resultBind) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_bind_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_store_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    while (true)
    {
        int fetchResult = mysql_stmt_fetch(stmt);

        if (fetchResult == MYSQL_NO_DATA)
        {
            break;
        }

        if (fetchResult != 0 && fetchResult != MYSQL_DATA_TRUNCATED)
        {
            // error 처리
            break;
        }

        // 여기서 변수들에 현재 row 값이 들어와 있음
        InventorySlot slot;
        slot.inventoryType = inventoryType;
        slot.slotPos = slotpos;
        slot.itemId = itemId;
        slot.itemCount = itemCount;

        auto inventory =inventoryManager->GetInventory(inventoryType);
        if(inventory == nullptr)
        {
            K_slog_trace(K_SLOG_ERROR, "inventory is nullptr");
            mysql_stmt_free_result(stmt);
            mysql_stmt_close(stmt);
            m_mySql->ReleaseConnection(conn);
            return false;
        }
        inventory->SetSlotItem(slotpos, itemId, itemCount);
    }
    mysql_stmt_close(stmt);
    m_mySql->ReleaseConnection(conn);
    return true;
}

bool PlayerService::LoadLearnedSkill(Player* player)
{

    int skill_id = 0;
    int skill_level = 0;
    MYSQL* conn = m_mySql->GetConnection(); 
    if(!conn) 
    {
        K_slog_trace(K_SLOG_ERROR, "conn is nullptr  ERROR [%s]", mysql_error(conn));
        return false;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_init ERROR [%s]", mysql_error(conn));
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    const char* query = "SELECT skill_id, skill_level FROM character_skill WHERE char_id = ?";
   
    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_prepare ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;

    }

    MYSQL_BIND param[1]{};
    int characterId = player->GetId();
    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &characterId;

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_bind_param ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_execute ERROR [%s]", mysql_stmt_error(stmt));
        K_slog_trace(K_SLOG_ERROR, "SQL [%s]", query);

        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;

    }

    MYSQL_BIND resultBind[2]{};

    resultBind[0].buffer_type = MYSQL_TYPE_LONG;
    resultBind[0].buffer = &skill_id;

    resultBind[1].buffer_type = MYSQL_TYPE_LONG;
    resultBind[1].buffer = &skill_level;

    if(mysql_stmt_bind_result(stmt, resultBind) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_bind_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;

    }

    if(mysql_stmt_store_result(stmt) !=0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_store_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;

    }


    while(true)
    {
        int fetchResult = mysql_stmt_fetch(stmt);

        if(fetchResult == MYSQL_NO_DATA)
        {
            break;
        }

        if(fetchResult != 0 && fetchResult != MYSQL_DATA_TRUNCATED)
        {
            break;
        }

        LearnedSkill learnedSkill{};

        learnedSkill.skill_id = skill_id;
        learnedSkill.skill_level = skill_level;

        player->SetLearnedSkill(learnedSkill);
    }

    mysql_stmt_close(stmt);
    m_mySql->ReleaseConnection(conn);
    return true;
}

bool PlayerService::LoadSlotSetting(Player* player)
{
    int result = 0;
    MYSQL_RES *res = nullptr;
    MYSQL_ROW row;
    MYSQL* conn = m_mySql->GetConnection(); 
    if(!conn) return false;

    /*
     int slot_index;
    QuickSlotType type;
    int ref_id;   // skill_id 또는 item_id 등
    int inventory_type;
    int inventory_slotPos;
    */
    std::string query = "SELECT slot_index, slot_type, ref_id, inventory_type, inventory_slotPos FROM character_skill_slot WHERE char_id = " + std::to_string(player->GetId());
    result = mysql_query(conn, query.c_str());
    if(result != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "LoadSlotSetting ERROR [%s]", mysql_error(conn));
        K_slog_trace(K_SLOG_ERROR, "SQL [%s]", query.c_str());
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    res = mysql_store_result(conn);
    if(!res)
    {
        K_slog_trace(K_SLOG_ERROR, "LoadSlotSetting ERROR [%s]", mysql_error(conn));
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    auto quickSlotManager = player->GetQuickSlotManager();

    if(!quickSlotManager)
    {
        K_slog_trace(K_SLOG_ERROR, "quickSlotManager is nullptr [%s]", mysql_error(conn));
        m_mySql->ReleaseConnection(conn);
        return false;
    }
    // 퀵슬롯 정보 저장
    while((row = mysql_fetch_row(res)) != nullptr)
    {
        QuickSlotData quickSlot;
        quickSlot.slot_index = std::atoi(row[0]);
        quickSlot.type = QuickSlot::SetSlotType(std::atoi(row[1]));
        quickSlot.ref_id = std::atoi(row[2]);
        quickSlot.inventory_type = std::stoi(row[3]);
        quickSlot.inventory_slotPos = std::stoi(row[4]);
        quickSlotManager->SetSlot(quickSlot);
    }

    mysql_free_result(res);
    m_mySql->ReleaseConnection(conn);
    return true;
}

bool PlayerService::LoadPlayerInfo(int characterId, PlayerInitData &playerInit)
{
    MYSQL* conn = m_mySql->GetConnection();
    if (!conn)
    {
        K_slog_trace(K_SLOG_ERROR, "conn is nullptr");
        return false;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_init ERROR [%s]", mysql_error(conn));
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    const char* query =
        "SELECT char_id, account_id, name, level, job, root_job "
        "FROM `character` WHERE char_id = ?";

    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_prepare ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    MYSQL_BIND param[1]{};

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &characterId;

    if (mysql_stmt_bind_param(stmt, param) != 0)
    {
        
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_execute ERROR [%s]", mysql_stmt_error(stmt));
        K_slog_trace(K_SLOG_ERROR, "SQL [%s]", query);

        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    char nameBuffer[64]{};
    unsigned long nameLength = 0;
    bool nameIsNull = 0;

    char accountIdBuffer[64]{};
    unsigned long accountIdLength = 0;
    bool accountIdIsNull = false;
    MYSQL_BIND resultBind[6]{};

    resultBind[0].buffer_type = MYSQL_TYPE_LONG;
    resultBind[0].buffer = &playerInit.char_id;

    resultBind[1].buffer_type = MYSQL_TYPE_STRING;
    resultBind[1].buffer = accountIdBuffer;
    resultBind[1].buffer_length = sizeof(accountIdBuffer);
    resultBind[1].length = &accountIdLength;
    resultBind[1].is_null = &accountIdIsNull;

    resultBind[2].buffer_type = MYSQL_TYPE_STRING;
    resultBind[2].buffer = nameBuffer;
    resultBind[2].buffer_length = sizeof(nameBuffer);
    resultBind[2].length = &nameLength;
    resultBind[2].is_null = &nameIsNull;

    resultBind[3].buffer_type = MYSQL_TYPE_LONG;
    resultBind[3].buffer = &playerInit.level;

    resultBind[4].buffer_type = MYSQL_TYPE_LONG;
    resultBind[4].buffer = &playerInit.job;

    resultBind[5].buffer_type = MYSQL_TYPE_LONG;
    resultBind[5].buffer = &playerInit.root_job;

    if (mysql_stmt_bind_result(stmt, resultBind) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_bind_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_store_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    int fetchResult = mysql_stmt_fetch(stmt);
    if (fetchResult == MYSQL_NO_DATA)
    {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (fetchResult != 0 && fetchResult != MYSQL_DATA_TRUNCATED)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_fetch ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (!accountIdIsNull)
    {
        playerInit.account_id.assign(accountIdBuffer, accountIdLength);
    }
    else
    {
        playerInit.account_id.clear();
    }

    if (!nameIsNull)
    {
        playerInit.name.assign(nameBuffer, nameLength);
    }
    else
    {
        playerInit.name.clear();
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    m_mySql->ReleaseConnection(conn);

    return true;
}

bool PlayerService::LoadPlayerStat(int characterId, AllStat &allStat)
{
    MYSQL* conn = m_mySql->GetConnection();
    if (!conn)
    {
        return false;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_init ERROR [%s]", mysql_error(conn));
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    const char* query =
        "SELECT str, dex, intel, luk, max_hp, max_mp, cur_hp, cur_mp, remain_ap, exp,level"
        " FROM character_stat WHERE char_id = ?";

    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_prepare ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    MYSQL_BIND param[1]{};

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &characterId;

    if (mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_bind_param ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_execute ERROR [%s]", mysql_stmt_error(stmt));
        K_slog_trace(K_SLOG_ERROR, "SQL [%s]", query);

        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    MYSQL_BIND resultBind[11]{};

    resultBind[0].buffer_type = MYSQL_TYPE_LONG;
    resultBind[0].buffer = &allStat.baseStat.str;

    resultBind[1].buffer_type = MYSQL_TYPE_LONG;
    resultBind[1].buffer = &allStat.baseStat.dex;

    resultBind[2].buffer_type = MYSQL_TYPE_LONG;
    resultBind[2].buffer = &allStat.baseStat.intel;
  
    resultBind[3].buffer_type = MYSQL_TYPE_LONG;
    resultBind[3].buffer = &allStat.baseStat.luck;

    resultBind[4].buffer_type = MYSQL_TYPE_LONG;
    resultBind[4].buffer = &allStat.derivedStat.maxHp;

    resultBind[5].buffer_type = MYSQL_TYPE_LONG;
    resultBind[5].buffer = &allStat.derivedStat.maxMp;

    resultBind[6].buffer_type = MYSQL_TYPE_LONG;
    resultBind[6].buffer = &allStat.curHp;

    resultBind[7].buffer_type = MYSQL_TYPE_LONG;
    resultBind[7].buffer = &allStat.curMp;

    resultBind[8].buffer_type = MYSQL_TYPE_LONG;
    resultBind[8].buffer = &allStat.remainAp;

    resultBind[9].buffer_type = MYSQL_TYPE_LONGLONG;
    resultBind[9].buffer = &allStat.expStat.exp;

    resultBind[10].buffer_type = MYSQL_TYPE_LONG;
    resultBind[10].buffer = &allStat.expStat.level;

    if (mysql_stmt_bind_result(stmt, resultBind) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_bind_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (mysql_stmt_store_result(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_store_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    int fetchResult = mysql_stmt_fetch(stmt);
    if (fetchResult == MYSQL_NO_DATA)
    {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    if (fetchResult != 0 && fetchResult != MYSQL_DATA_TRUNCATED)
    {
        K_slog_trace(K_SLOG_ERROR, "mysql_stmt_fetch ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    auto levelManager = LevelManager::GetInstance();

    if(levelManager == nullptr)
    {
        K_slog_trace(K_SLOG_ERROR, "levelManager is nullptr");
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);   
        return false;   
    }

    allStat.expStat.need_exp = levelManager->GetNeedExp(allStat.expStat.level);
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    m_mySql->ReleaseConnection(conn);
    
    return true;
}
