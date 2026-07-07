#include "PlayerManager.h"

PlayerManager::PlayerManager()
{
    m_mySql = MySqlConnectionPool::GetInstance();
}

PlayerManager::~PlayerManager()
{
}


bool PlayerManager::AddPlayer(std::unique_ptr<Player> player)
 {
     if (player == nullptr)
    {
        return false;
    }

    int playerId = player->GetId();

    std::lock_guard<std::mutex> lock(m_PlayerMutex);
    if(m_players.find(playerId) != m_players.end())
    {
        return false; 
    }
    K_slog_trace(K_SLOG_TRACE, " [%s][%d] Player character_id [%d]", __FUNCTION__ , __LINE__, player->GetId());
    K_slog_trace(K_SLOG_TRACE, " [%s][%d] Player character_id [%s]", __FUNCTION__ , __LINE__, player->GetName().c_str());

    m_players[playerId] = move(player);
    return true;
}




Player* PlayerManager::GetPlayer(int playerId)
{
    std::lock_guard<std::mutex> lock(m_PlayerMutex);
    auto it = m_players.find(playerId);

    if(it == m_players.end()) return nullptr;

    return it->second.get();
}

//추후에는 Redis먼저 조회 하고, Redis의 특정 주기에 따라서 DB조회를 해야함
//바로 DB조회 하는 방식은 리소스 손해를 많이본다.
Player* PlayerManager::GetPlayer(const std::string& playerName)
{
    int playerId = 0;

    MYSQL *conn = m_mySql->GetConnection();
    if(!conn)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d ] conn is nullptr", __FILE__, __FUNCTION__, __LINE__);
        return nullptr;
    }
    // 더 좋은 Connecter/C++이 있지만 DB연동 원리를 이해하기 위해서 C API의 Prepared Statement를 사용함
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d ] mysql_stmt_init ERROR [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_error(conn));
        m_mySql->ReleaseConnection(conn);
        return nullptr;
    }

    const char* query = "SELECT char_id from `character` WHERE name = ?";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d ] mysql_stmt_prepare ERROR [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return nullptr;
    }
   
    MYSQL_BIND param[1]{};

    unsigned long playerNameLen = static_cast<unsigned long>(playerName.size());

    param[0].buffer_type = MYSQL_TYPE_STRING;
    param[0].buffer = const_cast<char*>(playerName.c_str());
    // 버퍼의 크기
    param[0].buffer_length = playerNameLen;
    //length 실제 전송할 사이즈
    param[0].length = &playerNameLen;
    param[0].is_null = nullptr;

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d ] mysql_stmt_bind_param ERROR [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return nullptr;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d ] mysql_stmt_execute ERROR [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        K_slog_trace(K_SLOG_ERROR, "SQL [%s]", query);

        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return nullptr;
    }

    MYSQL_BIND resultBind[1]{};

    resultBind[0].buffer_type = MYSQL_TYPE_SHORT;
    resultBind[0].buffer = &playerId;

    if(mysql_stmt_bind_result(stmt,resultBind) !=0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d ] mysql_stmt_bind_result ERROR [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return nullptr;
    }

    if(mysql_stmt_store_result(stmt) != 0)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_store_result ERROR [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return nullptr;
    }

    int fetchResult = mysql_stmt_fetch(stmt);
    if(fetchResult == MYSQL_NO_DATA)
    {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return nullptr;
    }

    //MYSQL_DATA_TRUNCATED : 가져온 데이터가 짤리거나 범위를 초과했을 때 나오는 에러
    if(fetchResult !=0 && fetchResult != MYSQL_DATA_TRUNCATED)
    {
        K_slog_trace(K_SLOG_ERROR, "[%s : %s : %d] mysql_stmt_fetch ERROR [%s]", __FILE__, __FUNCTION__, __LINE__, mysql_stmt_error(stmt));
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        m_mySql->ReleaseConnection(conn);
        return nullptr;
    }
    Player* result = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_PlayerMutex);
        auto it = m_players.find(playerId);
        if (it != m_players.end())
        {
            result = it->second.get();
        }
    }
    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    m_mySql->ReleaseConnection(conn);
    return result;
}

bool PlayerManager::RemovePlayer(int playerId)
{
    std::lock_guard<std::mutex> lock(m_PlayerMutex);
    auto it = m_players.find(playerId);

    if(it == m_players.end()) return false;

    m_players.erase(it);
    return true;
}