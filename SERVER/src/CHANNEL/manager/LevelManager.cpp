#include "LevelManager.h"

LevelManager *LevelManager::m_instance =nullptr;
MySqlConnectionPool *LevelManager::m_mySql = nullptr;


LevelManager::LevelManager()
{
    m_mySql = MySqlConnectionPool::GetInstance();

}


LevelManager *LevelManager::GetInstance()
{
    if(m_instance == nullptr)
    {
        m_instance = new LevelManager();
        m_instance->Init();
    }
    return m_instance;
}

bool LevelManager::Init()
{
    if(!LoadLevelTable()) return false; 
    return true;
}

bool LevelManager::LoadLevelTable()
{
    int result = 0;
    MYSQL_RES *res = nullptr;
    MYSQL_ROW row;
    MYSQL* conn = m_mySql->GetConnection(); 

    if(!conn) return false;
    std::string query = "SELECT level, need_exp FROM level_exp";
    result = mysql_query(conn, query.c_str());
    if(result != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "LoadLevelTable ERROR [%s]", mysql_error(conn));
        K_slog_trace(K_SLOG_ERROR, "SQL [%s]", query.c_str());
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    res = mysql_store_result(conn);
    if(!res)
    {
        K_slog_trace(K_SLOG_ERROR, "LoadLevelTable ERROR [%s]", mysql_error(conn));
        m_mySql->ReleaseConnection(conn);
        return false;
    }

    while ((row = mysql_fetch_row(res)) != nullptr)
    {
        if (row[0] == nullptr || row[1] == nullptr)
            continue;

        int level = std::atoi(row[0]);
        int64_t needExp = std::atoll(row[1]);

        m_needExpTable[level] = needExp;
    }

    K_slog_trace(K_SLOG_TRACE, "LoadLevelTable Success");

    mysql_free_result(res);
    m_mySql->ReleaseConnection(conn);
    return true;
}

int64_t LevelManager::GetNeedExp(int level) const
{
    auto it = m_needExpTable.find(level);
    if(it == m_needExpTable.end())
    {
        K_slog_trace(K_SLOG_ERROR, "m_needExpTable is end Level [%d]",level);
        return 0;
    }

    return it->second;
}
