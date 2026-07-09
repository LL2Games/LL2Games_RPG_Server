#include "MySQLManager.h"
#include "MySqlConnectionPool.h"
#include "K_slog.h"
#include <cstring>

MySQLManager* MySQLManager::m_instance;

MySQLManager::MySQLManager()
{
    m_pool = MySqlConnectionPool::GetInstance();
}

MySQLManager* MySQLManager::GetInstance()
{
    if (m_instance == nullptr)
    {
        m_instance = new MySQLManager();
    }

    return m_instance;
}
 
std::string MySQLManager::GetNick(const std::string &id)
{
    std::string rcNick = "";
    MYSQL* conn = m_pool->GetConnection();

    unsigned long idLength = id.size();
    MYSQL_BIND param[1]{};
    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].buffer = const_cast<char*>(id.c_str());
    param[0].buffer_length = id.size();
    param[0].length = &idLength;

    char nickBuffer[64]{};
    unsigned long pwLength = 0;
    bool nickIsNull = false;
    MYSQL_BIND resultBind[1]{};

    int fetchResult = 0;

    if (conn == nullptr)
    {
        K_LOG_ERROR( "conn is nullptr (GetConnection ERROR)");
        return "";
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init ERROR [%s]", mysql_error(conn));
        m_pool->ReleaseConnection(conn);
        return "";
    }

    const char* query = "SELECT name from `character` WHERE char_id = ? ";
    

    if(mysql_stmt_prepare(stmt, query, strlen(query)) !=0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare ERROR [%s]", mysql_stmt_error(stmt));
        goto err;
    }

    

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param ERROR [%s]", mysql_stmt_error(stmt));
        goto err;
    }

    if(mysql_stmt_execute(stmt) !=0)
    {
        K_LOG_ERROR( "mysql_stmt_execute ERROR [%s]", mysql_stmt_error(stmt));
        K_LOG_ERROR( "SQL [%s]", query);
        goto err;
    }

    resultBind[0].buffer_type = MYSQL_TYPE_STRING;
    resultBind[0].buffer = nickBuffer;
    resultBind[0].buffer_length = sizeof(nickBuffer);
    resultBind[0].length = &pwLength;
    resultBind[0].is_null = &nickIsNull;

    if(mysql_stmt_bind_result(stmt, resultBind) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_result ERROR [%s]", mysql_stmt_error(stmt));
        goto err;
    }

    if(mysql_stmt_store_result(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_store_result ERROR [%s]", mysql_stmt_error(stmt));
        goto err;

    }

    fetchResult = mysql_stmt_fetch(stmt);

    if (fetchResult == MYSQL_NO_DATA)
    {
        K_LOG_ERROR( "No data found for account_id: %s", id.c_str());
        goto err;
    }

    if (fetchResult != 0 && fetchResult != MYSQL_DATA_TRUNCATED)
    {
        K_LOG_ERROR( "mysql_stmt_fetch ERROR [%s]", mysql_stmt_error(stmt));
        goto err;
    }

    if (!nickIsNull)
    {
        rcNick.assign(nickBuffer, pwLength);
    }

err:
    mysql_stmt_close(stmt);
    m_pool->ReleaseConnection(conn);
    return rcNick;
}
