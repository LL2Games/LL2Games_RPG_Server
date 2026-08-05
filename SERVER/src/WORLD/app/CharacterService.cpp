#include "CharacterService.h"
#include "K_slog.h"
#include "RedisClient.h"
#include "MySqlConnectionPool.h"
#include "RedisClient.h"
#include <string.h>

enum TTL {
    E_TTL_CHARLIST = 300,
};


CharacterService::CharacterService()
{
    m_db = MySqlConnectionPool::GetInstance();
}

CharacterService::~CharacterService()
{

}

std::vector<std::string> CharacterService::GetCharacterList(const std::string& account_id, RedisClient& redis)
{  
    long long charId =0;
    std::string name;
    int level = 0;
    int job = 0;
    std::vector<std::string> char_list;
    MYSQL* conn = nullptr;
    

    //test
    K_LOG_DEBUG( "id[%s]", account_id.c_str());

#if 0 //gunoo22 260805 캐릭터 리스트 Redis 캐싱때문에 캐릭터 생성후 캐릭터 리스트가 바로 안보이는 문제 발생 
    //1.Redis charlist 조회
    auto cached = redis.HGetAll("charlist:" + account_id);
    if (cached.has_value() && !cached->empty())
    {
        K_LOG_DEBUG( "redis cache hit for account_id[%s]", account_id.c_str());
        for (auto& [cid, value] : cached.value())
        {
            char_list.push_back(value);
        }
        return char_list;
    }
    
    K_LOG_DEBUG( "redis miss for account_id[%s]", account_id.c_str());
    #endif

    //2.Redis miss-> MySQL 조회
    conn = m_db->GetConnection();
    if (!conn)
    {
        K_LOG_ERROR( "MYSQL GetConnection failed");
        return char_list;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_error(conn));
        m_db->ReleaseConnection(conn);
        return char_list;
    }

    const char* query = "SELECT char_id, name, level, job FROM `character` WHERE account_id = ?";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_db->ReleaseConnection(conn);
        return char_list;
    }

    unsigned long accountIdLength =
    static_cast<unsigned long>(account_id.size());

    MYSQL_BIND param[1]{};

    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].buffer = const_cast<char*>(account_id.c_str());
    param[0].buffer_length = accountIdLength;
    param[0].length = &accountIdLength;

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_db->ReleaseConnection(conn);
        return char_list;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_execute Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_db->ReleaseConnection(conn);
        return char_list;
    }

    char nameBuffer[64]{};
    unsigned long nameLength = 0;
    bool nameIsNull = false;
    bool nameError = false;

    MYSQL_BIND resultBind[4]{};

    resultBind[0].buffer_type = MYSQL_TYPE_LONGLONG;
    resultBind[0].buffer = &charId;
  
    resultBind[1].buffer_type = MYSQL_TYPE_STRING;
    resultBind[1].buffer = nameBuffer;
    resultBind[1].buffer_length = sizeof(nameBuffer);
    resultBind[1].length = &nameLength;
    resultBind[1].is_null = &nameIsNull;
    resultBind[1].error = &nameError;

    resultBind[2].buffer_type = MYSQL_TYPE_LONG;
    resultBind[2].buffer = &level;

    resultBind[3].buffer_type = MYSQL_TYPE_LONG;
    resultBind[3].buffer = &job;

    if(mysql_stmt_bind_result(stmt, resultBind) !=0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_result Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_db->ReleaseConnection(conn);
        return char_list;
    }

    if(mysql_stmt_store_result(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_result Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_db->ReleaseConnection(conn);
        return char_list; 
    }

    //전체 캐릭터 수 먼저 추가
    char_list.push_back(std::to_string(mysql_stmt_num_rows(stmt)));
    
    while (true)
    {
        int fetchResult = mysql_stmt_fetch(stmt);

        if (fetchResult == MYSQL_NO_DATA)
        {
            break;
        }

        if (fetchResult != 0 && fetchResult != MYSQL_DATA_TRUNCATED)
        {
            break;
        }

        if (nameIsNull)
            name = "";
        else
            name.assign(nameBuffer, nameLength);

        std::string charIdStr = std::to_string(charId);
        std::string summary = charIdStr + "$" + name + "$" + std::to_string(level) + "$" + std::to_string(job);
        char_list.push_back(summary);

         //Redis캐시 적재
      
        redis.HSet("charlist:" + account_id, std::to_string(charId), summary, E_TTL_CHARLIST);  
    }
    
    mysql_stmt_close(stmt);
    m_db->ReleaseConnection(conn);
    //test
    for (auto list: char_list)
    {
        K_LOG_DEBUG( "list[%s]", list.c_str());
    }

    return char_list;
}

static int InsertCharacter(MYSQL_STMT* stmt, const std::string& account_id, const std::string& nick, int job, int root_job)
{
    const char* query = "INSERT INTO `character` "
    "(`account_id`, `name`, `job`, `root_job`, `created_at`) "
    "VALUES (?, ?, ?, ?, NOW())";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
        return EXIT_FAILURE;
    }

    unsigned long accountIdLength = static_cast<unsigned long>(account_id.size());
    unsigned long nickLength = static_cast<unsigned long>(nick.size());

    MYSQL_BIND param[4]{};

    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].buffer = const_cast<char*>(account_id.c_str());
    param[0].buffer_length = accountIdLength;
    param[0].length = &accountIdLength;

    param[1].buffer_type = MYSQL_TYPE_STRING;
    param[1].buffer = const_cast<char*>(nick.c_str());
    param[1].buffer_length = nickLength;
    param[1].length = &nickLength;

    param[2].buffer_type = MYSQL_TYPE_LONG;
    param[2].buffer = &job;

    param[3].buffer_type = MYSQL_TYPE_LONG;
    param[3].buffer = &root_job;

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param Error [%s]", mysql_stmt_error(stmt));
        return EXIT_FAILURE;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_execute Error [%s]", mysql_stmt_error(stmt));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int InsertCharacterStat(MYSQL_STMT* stmt, unsigned long long charId)
{
    const char* query =
        "INSERT INTO `character_stat` "
        "(`char_id`, `str`, `dex`, `intel`, `luk`, "
        "`max_hp`, `max_mp`, `cur_hp`, `cur_mp`, `remain_ap`) "
        "VALUES (?, 35, 4, 4, 4, 300, 100, 300, 100, 0)";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
        return EXIT_FAILURE;
    }

    MYSQL_BIND param[1]{};

    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer = &charId;
    param[0].is_unsigned = true;


    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param Error [%s]", mysql_stmt_error(stmt));
        return EXIT_FAILURE;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_execute Error [%s]", mysql_stmt_error(stmt));
        return EXIT_FAILURE;
    }

    K_LOG_TRACE(
        "Character stat inserted. char_id=[%llu]",
        charId);

    return EXIT_SUCCESS;
}


static int InsertCharacterInventoryMeta(MYSQL_STMT* stmt, unsigned long long charId)
{
    const char* query =
        "INSERT INTO `character_inventory_meta` "
        "(`char_id`, `inventory_type`, `max_slot`, `current_slot_count`) "
        "VALUES "
        "(?, 0, 128, 36), "
        "(?, 1, 128, 36), "
        "(?, 2, 128, 36), "
        "(?, 3, 128, 36), "
        "(?, 4, 128, 36)";

    if (mysql_stmt_prepare( stmt, query, static_cast<unsigned long>(strlen(query))) != 0)
    {
        K_LOG_ERROR(
            "mysql_stmt_prepare Error errno=[%u], error=[%s]",
            mysql_stmt_errno(stmt),
            mysql_stmt_error(stmt));

        return EXIT_FAILURE;
    }

    MYSQL_BIND param[5]{};

    for (int i = 0; i < 5; ++i)
    {
        param[i].buffer_type = MYSQL_TYPE_LONGLONG;
        param[i].buffer = &charId;
        param[i].is_unsigned = true;
    }

    if (mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR(
            "mysql_stmt_bind_param Error errno=[%u], error=[%s]",
            mysql_stmt_errno(stmt),
            mysql_stmt_error(stmt));

        return EXIT_FAILURE;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        K_LOG_ERROR(
            "mysql_stmt_execute Error "
            "errno=[%u], error=[%s], char_id=[%llu]",
            mysql_stmt_errno(stmt),
            mysql_stmt_error(stmt),
            charId);

        return EXIT_FAILURE;
    }

    if (mysql_stmt_affected_rows(stmt) != 5)
    {
        K_LOG_ERROR(
            "Unexpected affected rows. "
            "char_id=[%llu], affected_rows=[%llu]",
            charId,
            static_cast<unsigned long long>(
                mysql_stmt_affected_rows(stmt)));

        return EXIT_FAILURE;
    }

    K_LOG_TRACE(
        "Character inventory meta inserted. "
        "char_id=[%llu], inventory_type=[0~4]",
        charId);

    return EXIT_SUCCESS;
}

int CharacterService::CreateCharacter(const std::string& account_id, const std::string& nick, int job)
{
    //TODO
    //캐릭터 최대 갯수 예외처리 필요

    int rc = EXIT_FAILURE;

    MYSQL* conn = nullptr; 
    MYSQL_STMT* stmt = nullptr;
    std::string query;
    int root_job = 20000; //root_job은 일단 Knight로 고정
    unsigned long long charId;

    //1. MySQL 연결 가져오기
    conn = m_db->GetConnection();
    if (!conn)
    {
        K_LOG_ERROR( "MYSQL GetConnection failed");
        goto cleanup;
    }

    //2. Prepared Statement 생성
    stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_error(conn));
        goto cleanup;
    }

    //3. 'character' 테이블 INSERT
    if (InsertCharacter(stmt, account_id, nick, job, root_job) != EXIT_SUCCESS)
    {
        K_LOG_ERROR( "InsertCharacter failed");
        goto cleanup;
    }

    //3-2 INSERT 성공 시, 생성된 char_id를 가져온다.
     {
        charId = mysql_stmt_insert_id(stmt);

        K_LOG_TRACE(
            "Character created. char_id=[%llu], "
            "account_id=[%s], name=[%s], job=[%d]",
            charId,
            account_id.c_str(),
            nick.c_str(),
            job);
    }

    //4. 'character_stat' 테이블 INSERT
    if (InsertCharacterStat(stmt, charId) != EXIT_SUCCESS)
    {
        K_LOG_ERROR( "InsertCharacterStat failed");
        goto cleanup;
    }
    
    //5. 'character_inventory_meta' 테이블 INSERT
    if (InsertCharacterInventoryMeta(stmt, charId) != EXIT_SUCCESS)
    {
        K_LOG_ERROR( "InsertCharacterInventoryMeta failed");
        goto cleanup;
    }
    
    rc = EXIT_SUCCESS;
    
    //7. 자원 정리
cleanup:
    if (stmt)
    {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        stmt = nullptr;
    }

    if (conn)
    {
        m_db->ReleaseConnection(conn);
        conn = nullptr;
    }

    return rc;
}

int CharacterService::CheckDupNick(const std::string& nick)
{
    int rc = EXIT_FAILURE;

    MYSQL* conn = nullptr; 
    MYSQL_STMT* stmt = nullptr;
    std::string query;
    bool isDuplicate = false;

    //1. MySQL 연결 가져오기
    conn = m_db->GetConnection();
    if (!conn)
    {
        K_LOG_ERROR( "MYSQL GetConnection failed");
        goto err;
    }

    //2. Prepared Statement 생성
    stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_error(conn));
        goto err;
    }

    //3. 쿼리준비
    query = "SELECT 1 FROM `character` WHERE name = ? LIMIT 1";

    if(mysql_stmt_prepare(stmt, query.c_str(), query.size()) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
        goto err;
    }

    //4. 입력 파라미터 바인딩
    {
        MYSQL_BIND paramBind[1];
        memset(paramBind, 0x00, sizeof(paramBind));

        unsigned long nickLength = static_cast<unsigned long>(nick.size());
        
        paramBind[0].buffer_type = MYSQL_TYPE_STRING;
        paramBind[0].buffer = const_cast<char*>(nick.data());
        paramBind[0].buffer_length = nickLength;
        paramBind[0].length = &nickLength;

        if (mysql_stmt_bind_param(stmt, paramBind) != 0)
        {
            K_LOG_ERROR("mysql_stmt_bind_param Error [%s]", mysql_stmt_error(stmt));
            goto err;
        }
    }

    //5. 쿼리실행
    if (mysql_stmt_execute(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_execute Error [%s]", mysql_stmt_error(stmt));
        goto err;
    }

    //6. 조회 결과 저장
    if(mysql_stmt_store_result(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_store_result  Error [%s]", mysql_stmt_error(stmt));
        goto err;
    }
    // 7. 결과 행 개수 확인
    isDuplicate = mysql_stmt_num_rows(stmt) > 0;

    if (isDuplicate)
    {
        K_LOG_ERROR("닉네임 중복 [%s]", nick.c_str());
        goto err;
    }
    else
    {
        K_LOG_DEBUG("사용 가능한 닉네임 [%s]", nick.c_str());
    }

    //8. 자원 정리
    rc = EXIT_SUCCESS;

err:
    if (stmt)
    {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        stmt = nullptr;
    }

    if (conn)
    {
        m_db->ReleaseConnection(conn);
        conn = nullptr;
    }

    return rc;
}