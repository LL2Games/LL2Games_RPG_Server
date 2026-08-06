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

bool MySQLManager::Register(const std::string &id, const std::string &pw, const std::string &pw_check)
{
    // 기본 입력값 검사
    if (id.empty() || pw.empty())
    {
        K_LOG_ERROR("Register failed: empty input");
        return false;
    }

    // 비밀번호 확인
    if (pw != pw_check)
    {
        K_LOG_ERROR("Register failed: password mismatch. account_id=[%s]",
                    id.c_str());
        return false;
    }

    MYSQL* conn = m_pool->GetConnection();
    if (conn == nullptr)
    {
        K_LOG_ERROR("conn is nullptr (GetConnection ERROR)");
        return false;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (stmt == nullptr)
    {
        K_LOG_ERROR("mysql_stmt_init ERROR [%s]", mysql_error(conn));
        m_pool->ReleaseConnection(conn);
        return false;
    }

    bool result = false;

    const char* query =
        "INSERT INTO users (account_id, passwd) "
        "VALUES (?, ?)";

    unsigned long idLength =
        static_cast<unsigned long>(id.size());
    unsigned long pwLength =
        static_cast<unsigned long>(pw.size());

    MYSQL_BIND params[2]{};

    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR("mysql_stmt_prepare ERROR [%s]",
                    mysql_stmt_error(stmt));
        goto cleanup;
    }

    params[0].buffer_type   = MYSQL_TYPE_STRING;
    params[0].buffer        = const_cast<char*>(id.data());
    params[0].buffer_length = idLength;
    params[0].length        = &idLength;

    params[1].buffer_type   = MYSQL_TYPE_STRING;
    params[1].buffer        = const_cast<char*>(pw.data());
    params[1].buffer_length = pwLength;
    params[1].length        = &pwLength;

    if (mysql_stmt_bind_param(stmt, params) != 0)
    {
        K_LOG_ERROR("mysql_stmt_bind_param ERROR [%s]",
                    mysql_stmt_error(stmt));
        goto cleanup;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        const unsigned int errorCode = mysql_stmt_errno(stmt);

        // MySQL 중복 키 오류
        if (errorCode == 1062)
        {
            K_LOG_ERROR(
                "Register failed: duplicate account_id"
                "account_id=[%s]",
                id.c_str());
        }
        else
        {
            K_LOG_ERROR(
                "mysql_stmt_execute ERROR [%u][%s]",
                errorCode,
                mysql_stmt_error(stmt));

            K_LOG_ERROR("SQL [%s]", query);
        }

        goto cleanup;
    }

    if (mysql_stmt_affected_rows(stmt) != 1)
    {
        K_LOG_ERROR(
            "Register failed: affected rows is not 1. rows=[%llu]",
            static_cast<unsigned long long>(
                mysql_stmt_affected_rows(stmt)));

        goto cleanup;
    }

    K_LOG_TRACE( "Register success. account_id=[%s]", id.c_str());

    result = true;

cleanup:
    mysql_stmt_close(stmt);
    m_pool->ReleaseConnection(conn);

    return result;
}