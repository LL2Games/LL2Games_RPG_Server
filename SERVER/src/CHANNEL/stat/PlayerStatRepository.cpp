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
        K_LOG_ERROR( "MYSQL GetConnection failed Error");
        errMsg = "MYSQL GetConnection failed";
        return -1;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init failed Error [%s] ", mysql_error(conn));
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
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
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
        K_LOG_ERROR( "mysql_stmt_bind_param Error [%s]", mysql_stmt_error(stmt));
        errMsg = "mysql_stmt_bind_param Error";
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    if(mysql_stmt_execute(stmt) !=0)
    {
        K_LOG_ERROR( "mysql_stmt_execute Error [%s]", mysql_stmt_error(stmt));
        errMsg = "mysql_stmt_execute Error";
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    my_ulonglong affectedRows = mysql_stmt_affected_rows(stmt);

    if (affectedRows == 0)
    {
        K_LOG_ERROR( "update affected 0 rows");
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }
    mysql_stmt_close(stmt);
    m_mySql->ReleaseConnection(conn);
    return 0;
}

int PlayerStatRepository::UpdateExpLevel(int charId, int level, int64_t exp, int remainAp, std::string& errMsg)
{
     MYSQL* conn = m_mySql->GetConnection();
    if (!conn)
    {
        errMsg = "MYSQL GetConnection failed";
        return -1;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt)
    {
        errMsg = mysql_error(conn);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    const char* query =
        "UPDATE character_stat "
        "SET level = ?, exp = ?, remain_ap = ? "
        "WHERE char_id = ?";

    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        errMsg = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    MYSQL_BIND param[4]{};

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &level;

    param[1].buffer_type = MYSQL_TYPE_LONGLONG;
    param[1].buffer = &exp;

    param[2].buffer_type = MYSQL_TYPE_LONG;
    param[2].buffer = &remainAp;

    param[3].buffer_type = MYSQL_TYPE_LONG;
    param[3].buffer = &charId;

    if (mysql_stmt_bind_param(stmt, param) != 0)
    {
        errMsg = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        errMsg = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    mysql_stmt_close(stmt);
    m_mySql->ReleaseConnection(conn);
    return 0;
}

int PlayerStatRepository::SaveRuntimeStat(int charId, const CharacterStat& stat, std::string& errMsg)
{
    MYSQL* conn = m_mySql->GetConnection();
    if (!conn)
    {
        errMsg = "MYSQL GetConnection failed";
        return -1;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt)
    {
        errMsg = mysql_error(conn);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    const char* query =
        "UPDATE character_stat "
        "SET cur_hp = ?, cur_mp = ?, exp = ?, level = ?, remain_ap = ? "
        "WHERE char_id = ?";

    if (mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        errMsg = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    int curHp = stat.GetCurHp();
    int curMp = stat.GetCurMp();
    long long exp = static_cast<long long>(stat.GetExp());
    int level = stat.GetLevel();
    int remainAp = stat.GetRemainAp();

    MYSQL_BIND param[6]{};

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &curHp;

    param[1].buffer_type = MYSQL_TYPE_LONG;
    param[1].buffer = &curMp;

    param[2].buffer_type = MYSQL_TYPE_LONGLONG;
    param[2].buffer = &exp;

    param[3].buffer_type = MYSQL_TYPE_LONG;
    param[3].buffer = &level;

    param[4].buffer_type = MYSQL_TYPE_LONG;
    param[4].buffer = &remainAp;

    param[5].buffer_type = MYSQL_TYPE_LONG;
    param[5].buffer = &charId;

    if (mysql_stmt_bind_param(stmt, param) != 0)
    {
        errMsg = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        errMsg = mysql_stmt_error(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    mysql_stmt_close(stmt);
    m_mySql->ReleaseConnection(conn);
    return 0;
}