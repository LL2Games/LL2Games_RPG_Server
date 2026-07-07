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
    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] id[%s]", __FUNCTION__, __LINE__, account_id.c_str());

    //1.Redis charlist 조회
   
    auto cached = redis.HGetAll("charlist:" + account_id);
    if (cached.has_value() && !cached->empty())
    {
        K_slog_trace(K_SLOG_DEBUG, "[%s][%d] redis cache hit for account_id[%s]", __FUNCTION__, __LINE__, account_id.c_str());
        for (auto& [cid, value] : cached.value())
        {
            char_list.push_back(value);
        }
        return char_list;
    }
    
    K_slog_trace(K_SLOG_DEBUG, "[%s][%d] redis miss for account_id[%s]", __FUNCTION__, __LINE__, account_id.c_str());

    //2.Redis miss-> MySQL 조회
    conn = m_db->GetConnection();
    if (!conn)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s][%d] MYSQL GetConnection failed", __FUNCTION__, __LINE__);
        return char_list;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_prepare Error [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_error(conn));
        m_db->ReleaseConnection(conn);
        return char_list;
    }

    const char* query = "SELECT char_id, name, level, job FROM `character` WHERE account_id = ?";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_prepare Error [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
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
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_bind_param Error [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_db->ReleaseConnection(conn);
        return char_list;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_execute Error [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
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
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_bind_result Error [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_db->ReleaseConnection(conn);
        return char_list;
    }

    if(mysql_stmt_store_result(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_bind_result Error [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
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
        K_slog_trace(K_SLOG_DEBUG, "[%s][%d] list[%s]", __FUNCTION__, __LINE__, list.c_str());
    }

    return char_list;
}