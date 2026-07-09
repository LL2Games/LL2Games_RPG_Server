#include "MySQLManager.h"
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

bool MySQLManager::Login(const std::string &id, const std::string& pw)
{
    bool rc = true;
    MYSQL* conn = m_pool->GetConnection();
    if (conn == nullptr)
    {
        K_LOG_ERROR( "conn is nullptr (GetConnection ERROR)");
        return false;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init ERROR [%s]", mysql_error(conn));
        m_pool->ReleaseConnection(conn);
        return false;
    }

    const char* query = "SELECT passwd FROM users WHERE account_id =? ";
    
    if(mysql_stmt_prepare(stmt, query, strlen(query)) !=0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_pool->ReleaseConnection(conn);
        return false;
    }

    unsigned long idLength = id.size();

    MYSQL_BIND param[1]{};
    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].buffer = const_cast<char*>(id.c_str());
    param[0].buffer_length = id.size();
    param[0].length = &idLength;

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_pool->ReleaseConnection(conn);
        return false;
    }

    if(mysql_stmt_execute(stmt) !=0)
    {
        K_LOG_ERROR( "mysql_stmt_execute ERROR [%s]", mysql_stmt_error(stmt));
        K_LOG_ERROR( "SQL [%s]", query);

        mysql_stmt_close(stmt);
        m_pool->ReleaseConnection(conn);
        return false;
    }

    char pwBuffer[64]{};
    unsigned long pwLength = 0;
    bool pwIsNull = false;
    int fetchResult = 0;

    MYSQL_BIND resultBind[1]{};

    resultBind[0].buffer_type = MYSQL_TYPE_STRING;
    resultBind[0].buffer = pwBuffer;
    resultBind[0].buffer_length = sizeof(pwBuffer);
    resultBind[0].length = &pwLength;
    resultBind[0].is_null = &pwIsNull;

    if(mysql_stmt_bind_result(stmt, resultBind) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_result ERROR [%s]", mysql_stmt_error(stmt));
        rc = false;
        goto err;
    }

    if(mysql_stmt_store_result(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_store_result ERROR [%s]", mysql_stmt_error(stmt));
        rc = false;
        goto err;

    }

    fetchResult = mysql_stmt_fetch(stmt);

    if (fetchResult == MYSQL_NO_DATA)
    {
        K_LOG_ERROR( "No data found for account_id: %s", id.c_str());
        rc = false;
        goto err;
    }

    if (fetchResult != 0 && fetchResult != MYSQL_DATA_TRUNCATED)
    {
        K_LOG_ERROR( "mysql_stmt_fetch ERROR [%s]", mysql_stmt_error(stmt));
        rc = false;
        goto err;
    }

    if (!pwIsNull)
    {
        std::string dbPw(pwBuffer, pwLength);
        rc = (dbPw == pw);
    }

err:
    mysql_stmt_close(stmt);
    m_pool->ReleaseConnection(conn);
    return rc;
}