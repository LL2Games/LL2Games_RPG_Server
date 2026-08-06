#include "CharacterService.h"
#include "K_slog.h"
#include "RedisClient.h"
#include "MySqlConnectionPool.h"
#include "RedisClient.h"
#include <string.h>
#include <cstring>

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

bool CharacterService::OwnsCharacter(const std::string& account_id, int char_id)
{
     if (account_id.empty() || char_id <= 0)
    {
        return false;
    }

    MYSQL* connection = m_db->GetConnection();

    if (connection == nullptr)
    {
        K_LOG_ERROR("OwnsCharacter failed: database connection acquire failed");
        return false;
    }

    MYSQL_STMT* statement =mysql_stmt_init(connection);

    if (statement == nullptr)
    {
        K_LOG_ERROR("OwnsCharacter failed: statement initialization failed");
        m_db->ReleaseConnection(connection);
        return false;
    }

    const char* query ="SELECT 1 FROM `character` WHERE account_id = ? AND char_id = ? LIMIT 1";

    if (mysql_stmt_prepare(statement, query, static_cast<unsigned long>(std::strlen(query))) != 0)
    {
        K_LOG_ERROR("OwnsCharacter failed: statement prepare failed [%s]",mysql_stmt_error(statement));
        mysql_stmt_close(statement);
        m_db->ReleaseConnection(connection);
        return false;
    }

    unsigned long accountIdLength = static_cast<unsigned long>(account_id.size());

    long long characterIdValue = static_cast<long long>(char_id);

    MYSQL_BIND parameters[2]{};

    parameters[0].buffer_type = MYSQL_TYPE_STRING;
    parameters[0].buffer = const_cast<char*>(account_id.data());
    parameters[0].buffer_length = accountIdLength;
    parameters[0].length = &accountIdLength;

    parameters[1].buffer_type = MYSQL_TYPE_LONGLONG;
    parameters[1].buffer = &characterIdValue;

    if (mysql_stmt_bind_param(statement, parameters) != 0)
    {
        K_LOG_ERROR("OwnsCharacter failed: parameter binding failed [%s]",mysql_stmt_error(statement));
        mysql_stmt_close(statement);
        m_db->ReleaseConnection(connection);
        return false;
    }

    if (mysql_stmt_execute(statement) != 0)
    {
        K_LOG_ERROR("OwnsCharacter failed: execution failed [%s]",mysql_stmt_error(statement));
        mysql_stmt_close(statement);
        m_db->ReleaseConnection(connection);
        return false;
    }

    int ownershipResult = 0;
    MYSQL_BIND result[1]{};

    result[0].buffer_type = MYSQL_TYPE_LONG;
    result[0].buffer = &ownershipResult;

    if (mysql_stmt_bind_result(statement, result) != 0)
    {
        K_LOG_ERROR("OwnsCharacter failed: result binding failed [%s]", mysql_stmt_error(statement));
        mysql_stmt_close(statement);
        m_db->ReleaseConnection(connection);
        return false;
    }

    const int fetchResult = mysql_stmt_fetch(statement);
    const bool ownsCharacter = fetchResult == 0 && ownershipResult == 1;
    mysql_stmt_close(statement);
    m_db->ReleaseConnection(connection);

    return ownsCharacter;
}