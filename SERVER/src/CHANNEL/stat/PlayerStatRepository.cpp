#include "PlayerStatRepository.h"
#include "MySqlConnectionPool.h"
#include "RedisClient.h"
#include "K_slog.h"

PlayerStatRepository::PlayerStatRepository()
{
    m_mySql = MySqlConnectionPool::GetInstance();
}

PlayerStatRepository::~PlayerStatRepository()
{
}


//추후에는 Redis먼저 업데이트 하고, Redis의 특정 주기에 따라서 DB업데이트를 해야함
//바로 DB업데이트 하는 방식은 리소스 손해를 많이본다.
int PlayerStatRepository::Update(const std::string &charId, const std::string &statType, std::string &errMsg)
{
    MYSQL *conn = m_mySql->GetConnection();
    if (!conn)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] MYSQL GetConnection failed Error", __FILE__, __FUNCTION__, __LINE__);
        errMsg = "MYSQL GetConnection failed";
        return -1;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_init failed Error [%s] ", __FILE__, __FUNCTION__, __LINE__, mysql_error(conn));
        errMsg = mysql_error(conn);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    std::string column;

    if (statType == "str")
        column = "str";
    else if (statType == "dex")
        column = "dex";
    else if (statType == "int")
        column = "int";
    else if (statType == "luk")
        column = "luk";
    else
    {
        errMsg = "invalid statType";
        m_mySql->ReleaseConnection(conn);
        mysql_stmt_close(stmt);
        return -1;
    }

    std::string query = "UPDATE character_stat SET " + column + " = " + column + " + 1 WHERE char_id = ?";
    
    if(mysql_stmt_prepare(stmt, query.c_str(), query.size()) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_prepare Error [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        errMsg = "mysql_stmt_prepare Error";
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    long long char_id = std::atoll(charId.c_str());
    MYSQL_BIND param[1]{};

    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer = &char_id;

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_bind_param Error [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        errMsg = "mysql_stmt_bind_param Error";
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    if(mysql_stmt_execute(stmt) !=0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_execute Error [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        errMsg = "mysql_stmt_execute Error";
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    my_ulonglong affectedRows = mysql_stmt_affected_rows(stmt);

    if (affectedRows == 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] update affected 0 rows", __FILE__ ,__FUNCTION__, __LINE__);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }
    mysql_stmt_close(stmt);
    m_mySql->ReleaseConnection(conn);
    return 0;
}
