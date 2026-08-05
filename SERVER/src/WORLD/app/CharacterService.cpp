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


int CharacterService::CreateCharacter(const std::string& account_id, const std::string& nick, int job)
{
    //TODO
    //캐릭터 최대 갯수 예외처리 필요

    int rc = EXIT_FAILURE;

    MYSQL* conn = nullptr; 
    MYSQL_STMT* stmt = nullptr;
    std::string query;
    int root_job = job; //root_job을 일단 job으로 설정

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

    //3. 쿼리준비
    query =
    "INSERT INTO `character` "
    "(`account_id`, `name`, `job`, `root_job`, `created_at`) "
    "VALUES (?, ?, ?, ?, NOW())";
    //query = "SELECT 1 FROM `character` WHERE name = ? LIMIT 1";

    if(mysql_stmt_prepare(stmt, query.c_str(), query.size()) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
        goto cleanup;
    }

    //4. 입력 파라미터 바인딩
    {
        MYSQL_BIND paramBind[4]{};
        memset(paramBind, 0x00, sizeof(paramBind));

        unsigned long nickLength = static_cast<unsigned long>(nick.size());
        unsigned long account_idLength = static_cast<unsigned long>(account_id.size());
        unsigned long jobLength = sizeof(job);
        unsigned long root_jobLength = sizeof(root_job);

        paramBind[0].buffer_type = MYSQL_TYPE_STRING;
        paramBind[0].buffer = const_cast<char*>(account_id.data());
        paramBind[0].buffer_length = account_idLength;
        paramBind[0].length = &account_idLength;

        paramBind[1].buffer_type = MYSQL_TYPE_STRING;
        paramBind[1].buffer = const_cast<char*>(nick.data());
        paramBind[1].buffer_length = nickLength;
        paramBind[1].length = &nickLength;

        paramBind[2].buffer_type = MYSQL_TYPE_LONG;
        paramBind[2].buffer = &job;
        paramBind[2].buffer_length = jobLength;
        paramBind[2].length = &jobLength;

        paramBind[3].buffer_type = MYSQL_TYPE_LONG;
        paramBind[3].buffer = &root_job;
        paramBind[3].buffer_length = root_jobLength;
        paramBind[3].length = &root_jobLength;

        if (mysql_stmt_bind_param(stmt, paramBind) != 0)
        {
            K_LOG_ERROR("mysql_stmt_bind_param Error [%s]", mysql_stmt_error(stmt));
            goto cleanup;
        }
    }

    //5. 쿼리실행
    if (mysql_stmt_execute(stmt) != 0)
    {
        const unsigned int errorCode = mysql_stmt_errno(stmt);

        // ER_DUP_ENTRY
        // 닉네임 중복검사 이후 다른 요청이 같은 닉네임을 먼저
        // 생성했을 때 여기에서 최종적으로 차단된다.
        if (errorCode == 1062)
        {
            K_LOG_ERROR(
                "Character nickname already exists. "
                "account_id=[%s], name=[%s]",
                account_id.c_str(),
                nick.c_str());

            //rc = 1;
            goto cleanup;
        }

        // 존재하지 않는 account_id 등 외래키 오류
        if (errorCode == 1452)
        {
            K_LOG_ERROR(
                "Invalid account_id. account_id=[%s], error=[%s]",
                account_id.c_str(),
                mysql_stmt_error(stmt));

            //rc = 2;
            goto cleanup;
        }

        K_LOG_ERROR(
            "mysql_stmt_execute Error code=[%u], message=[%s]",
            errorCode,
            mysql_stmt_error(stmt));

        goto cleanup;
    }

    //6 INSERT 성공 시, 생성된 char_id를 가져온다.
     {
        const unsigned long long charId =
            mysql_stmt_insert_id(stmt);

        K_LOG_TRACE(
            "Character created. char_id=[%llu], "
            "account_id=[%s], name=[%s], job=[%d]",
            charId,
            account_id.c_str(),
            nick.c_str(),
            job);
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