#include "TradeService.h"
//#include "TradeRequestTask.h"
#include "MapInstance.h"
#include "MySqlConnectionPool.h"
//#include "RedisClient.h"
#include "K_slog.h"

std::mutex TradeService::m_TradeMutex;
std::unordered_map<int, TradeSession *> TradeService::m_sessions;



TradeService::TradeService()
{
    m_mySql = MySqlConnectionPool::GetInstance();
    //m_redis = RedisClient::GetInstance();
}
TradeService::~TradeService()
{
}

// int TradeService::HandleTradeRequest(Player* requester, const std::string& target_player_name)
int TradeService::Request(Player *requester, Player *target_player, std::string &errMsg)
{
    // 1. 예외처리: target_player와 requester 객체가 유효한지
    if (requester == nullptr || target_player == nullptr)
    {
        K_LOG_ERROR( "Invalid player or target player");
        errMsg = "Invalid player or target player";
        return -1;
    }

    // 2. 예외처리: target_player가 거래 가능한 상태인지 (ex. 전투중, 피격중, 사망 등등)
    if (!target_player->IsAlive())
    {
        K_LOG_ERROR( "Target player is not alive");
        errMsg = "Target player is not alive";
        return -1;
    }

    // 3. target_player에게 거래 요청 패킷 보내기
    target_player->GetSession()->Send(PKT_TRADE_REQUEST, {std::to_string(requester->GetId()), requester->GetName()});
    K_LOG_DEBUG( "Trade requester player id: [%d], name: [%s]", requester->GetId(), requester->GetName().c_str());

    return 0;
}

int TradeService::Start(Player *requester, Player *accepter, std::string &errMsg)
{
    K_LOG_DEBUG( "gunoo22_TEST");

    // 1. 예외처리: target_player와 requester 객체가 유효한지
    if (requester == nullptr || accepter == nullptr)
    {
        K_LOG_ERROR( "Invalid player or target player");
        errMsg = "Invalid player or target player";
        return -1;
    }
        K_LOG_DEBUG( "gunoo22_TEST");


    // 2-1. 예외처리: accepter와 requester가 맵 안에 있는지
    if (!(requester->GetCurrentMap() && accepter->GetCurrentMap()))
    {
        K_LOG_ERROR( "players mapInstance Invalid");
        errMsg = "players mapInstance Invalid";
        return -1;
    }

    // 2-2. 예외처리: accepter와 requester 같은 맵에 있는지
    if (requester->GetCurrentMap()->GetMapId() != accepter->GetCurrentMap()->GetMapId())
    {
        K_LOG_ERROR( "Players map different");
        errMsg = "Players map different";
        return -1;
    }

    // 3. 예외처리: accepter와 requester가 거래 가능한 상태인지 (ex. 전투중, 피격중, 사망 등등)
    if (!requester->IsAlive() || !accepter->IsAlive())
    {
        K_LOG_ERROR( "player is not alive");
        errMsg = "player is not alive";
        return -1;
    }

    // 4. TradeSession 생성
    CreateTradeSession(requester, accepter);
    K_LOG_DEBUG( "gunoo22_TEST CreateTradeSession");

    // 5. player들에게 교환 실행 패킷 보내기
    requester->GetSession()->Send(PKT_TRADE_START, {std::to_string(accepter->GetId()), accepter->GetName()});
    accepter->GetSession()->Send(PKT_TRADE_START, {std::to_string(requester->GetId()), requester->GetName()});
    K_LOG_DEBUG( "gunoo22_TEST PKT_TRADE_START->[name:%s]", accepter->GetName().c_str());
    K_LOG_DEBUG( "gunoo22_TEST PKT_TRADE_START->[name:%s]", requester->GetName().c_str());


    return 0;
}

int TradeService::UploadItem(Player* player, const TradeItem& item, std::string &errMsg)
{
    if (player == nullptr)
    {
        errMsg = "Invalid player";
        return -1;
    }
    std::lock_guard<std::mutex> lock(m_TradeMutex);

    auto it = m_sessions.find(player->GetId());
    if (it == m_sessions.end() || it->second == nullptr)
    {
        errMsg = "player trade session Not Found";
        return -1;
    }

    TradeSession* session = it->second;

    if (session->a_player == player)
        session->a_items.push_back(item);
    else
        session->b_items.push_back(item);
    return 0;
}

int TradeService::Ready(Player* player, const std::vector<TradeItem>& , std::string &errMsg)
{
    if (player == nullptr)
    {
        errMsg = "Invalid player";
        return -1;
    }

    TradeExecuteData executeData;
    bool shouldExecute = false;

    {
        std::lock_guard<std::mutex> lock(m_TradeMutex);

        auto it = m_sessions.find(player->GetId());
        if (it == m_sessions.end() || it->second == nullptr)
        {
            errMsg = "player trade session Not Found";
            return -1;
        }

        TradeSession* session = it->second;

        if (session->a_player == player)
            session->a_ready = true;
        else
            session->b_ready = true;

        shouldExecute =
            (session->a_id == player->GetId() && session->b_ready) ||
            (session->b_id == player->GetId() && session->a_ready);

        if (shouldExecute)
        {
            executeData.a_id = session->a_id;
            executeData.b_id = session->b_id;
            executeData.a_items = session->a_items;
            executeData.b_items = session->b_items;

            //m_sessions.erase(session->a_id);
            //m_sessions.erase(session->b_id);
            //delete session;
        }
    }

    if (shouldExecute)
        return Execute(executeData);

    return 2;
}

int TradeService::Execute(TradeSession *session)
{
    MYSQL *conn = m_mySql->GetConnection();
    std::string query;
    int result = 0;

    if (session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr");
        return -1;
    }

    if (!conn)
    {
        K_LOG_ERROR( "MYSQL GetConnection failed");
        return -1;
    }

    mysql_query(conn, "START TRANSACTION");
    K_LOG_DEBUG( "TRANSACTION");

    //0. 아이템 검증(예외처리)
    //A player DB에서 A items 존재 및 수량 확인
    //B player DB에서 B items 존재 및 수량 확인

    //1. 아이템 교환
    //A player DB에서 A items 임시 제거
    //B player DB에서 B items 임시제거

    //2. 교환 실패(인벤창 예외처리)
    //A player DB에 B items 추가시 인벤창 여유 확인
    //B player DB에 A items 추가시 인벤창 여유 확인

    //3. 교환 성사
    //A player DB에 A items 삭제 및 B items 추가
    //B player DB에 B items 삭제 및 A items 추가

    for (auto& item : session->a_items)
    {
        //3-1 A인벤에 A의 아이템 삭제
        if (DecreaseItem(conn, std::to_string(session->a_id), item) != 0)
        {
            K_LOG_ERROR( "DecreaseItem failed");
            result = -1;
            goto err;
        }

        //3-2 B인벤에 A의 아이템 추가
        if (IncreaseItem(conn, std::to_string(session->b_id), item) != 0)
        {
            K_LOG_ERROR( "IncreaseItem failed");
            result = -1;
            goto err;
        }
    }

    for (auto& item : session->b_items)
    {
        //3-3 B인벤에 B의 아이템 삭제
        if (DecreaseItem(conn, std::to_string(session->b_id), item) != 0)
        {
            K_LOG_ERROR( "DecreaseItem failed");
            result = -1;
            goto err;
        } 
    
        //3-4 A인벤에 B의 아이템 추가
        if (IncreaseItem(conn, std::to_string(session->a_id), item) != 0)
        {
            K_LOG_ERROR( "IncreaseItem failed");
            result = -1;
            goto err;
        }

    }

err:
    if (result != 0)
    {
        mysql_query(conn, "ROLLBACK");
        K_LOG_DEBUG( "ROLLBACK");
        return -1;
    }
    else
    {
        mysql_query(conn, "COMMIT");

        // //4. 교환세션 삭제
        // K_LOG_DEBUG( "COMMIT");
        // DeleteTradeSession(session); 
    }

    return 0;
}

int TradeService::Execute(TradeExecuteData& data)
{
     MYSQL* conn = m_mySql->GetConnection();

    if (!conn)
    {
        K_LOG_ERROR( "MYSQL GetConnection failed");
        return -1;
    }

    int result = 0;

    if (mysql_query(conn, "START TRANSACTION") != 0)
    {
        K_LOG_ERROR( "START TRANSACTION failed");
        m_mySql->ReleaseConnection(conn);
        return -1;
    }

    for (auto& item : data.a_items)
    {
        if (DecreaseItem(conn, std::to_string(data.a_id), item) != 0 ||
            IncreaseItem(conn, std::to_string(data.b_id), item) != 0)
        {
            result = -1;
            break;
        }
    }

    if (result == 0)
    {
        for (auto& item : data.b_items)
        {
            if (DecreaseItem(conn, std::to_string(data.b_id), item) != 0 ||
                IncreaseItem(conn, std::to_string(data.a_id), item) != 0)
            {
                result = -1;
                break;
            }
        }
    }

    if (result == 0)
    {
        if (mysql_query(conn, "COMMIT") != 0)
            result = -1;
    }
    else
    {
        mysql_query(conn, "ROLLBACK");
    }

    m_mySql->ReleaseConnection(conn);
    return result;
}

int TradeService::Cancel(Player *requester, std::string &errMsg)
{
    // 1. 예외처리: target_player와 requester 객체가 유효한지
    if (requester == nullptr)
    {
        K_LOG_ERROR( "Invalid player");
        errMsg = "Invalid player";
        return -1;
    }

    std::lock_guard<std::mutex> lock(m_TradeMutex);
    auto it = m_sessions.find(requester->GetId());
    // 2. 예외처리: player의 TradeSession 유효하지않음
    if (it == m_sessions.end() || it->second == nullptr)
    {
         K_LOG_ERROR( "player trade session Not Found"); 
        errMsg = "player trade session Not Found";
        return -1;
    }

    TradeSession* session = it->second; 

    // 기존에 DecreaseSession을 호출했지만 DecreaseSession 내부에서도 m_session 접근을 위해 
    // lock을 걸다 보니 같은 변수에 대한 2번의 락을 걸게 되어 deadlock이 발생할 수 있어
    // DecreaseSession 호출 없이 이 함수 내에서 삭제를 진행한다.
    m_sessions.erase(session->a_id);
    m_sessions.erase(session->b_id);

    delete session;

    return 0;
}

void TradeService::CreateTradeSession(Player *a_player, Player *b_player)
{
    if (a_player == nullptr || b_player == nullptr)
    {
        K_LOG_ERROR( "a_player or b_player is nullptr");
        return;
    }

    TradeSession *session = new TradeSession();

    session->a_player = a_player;
    session->b_player = b_player;
    session->a_id = a_player->GetId();
    session->b_id = b_player->GetId();
    std::lock_guard<std::mutex> lock(m_TradeMutex);
    if (m_sessions.find(a_player->GetId()) != m_sessions.end() ||
    m_sessions.find(b_player->GetId()) != m_sessions.end())
    {
        delete session;
        return;
    }
    m_sessions[a_player->GetId()] = session;
    m_sessions[b_player->GetId()] = session;
}

void TradeService::DeleteTradeSession(TradeSession *session)
{
    if (session == nullptr)
    {
        K_LOG_ERROR( "session is nullptr");
        return;
    }
    std::lock_guard<std::mutex> lock(m_TradeMutex);
    m_sessions.erase(session->a_id);
    m_sessions.erase(session->b_id);

    delete session;
}
//당장 불필요
TradeSession *TradeService::GetTradeSession(Player *player)
{
    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_TradeMutex);
    auto it = m_sessions.find(player->GetId());
    if (it == m_sessions.end())
    {
        K_LOG_ERROR( "player_id(%d) TradeSession not found", player->GetId());
        return nullptr;
    }

    return it->second;
}

Player* TradeService::GetTargetPlayer(Player *player)
{
    Player* target_player = nullptr;

    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_TradeMutex);
    auto it = m_sessions.find(player->GetId());
    if (it == m_sessions.end())
    {
        K_LOG_ERROR( "player_id(%d) TradeSession not found", player->GetId());
        return nullptr;
    }

    TradeSession* session = it->second;
    if (session->a_player == player)
        target_player = session->b_player;
    else
        target_player = session->a_player;

    return target_player;
}

const std::vector<TradeItem>& TradeService::GetMyItems(Player *player)
{
    static const std::vector<TradeItem> nullVector;
    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr");
        return nullVector;
    }

    std::lock_guard<std::mutex> lock(m_TradeMutex);
    auto it = m_sessions.find(player->GetId());
    if (it == m_sessions.end())
    {
        K_LOG_ERROR( "player_id(%d) TradeSession not found", player->GetId());
        return nullVector;
    }

    TradeSession* session = it->second;
    if (player == session->a_player)
        return session->a_items;
    else
        return session->b_items;
}

const std::vector<TradeItem>& TradeService::GetTargetItems(Player *player)
{
    static const std::vector<TradeItem> nullVector;
    if (player == nullptr)
    {
        K_LOG_ERROR( "player is nullptr");
        return nullVector;
    }

    std::lock_guard<std::mutex> lock(m_TradeMutex);
    auto it = m_sessions.find(player->GetId());
    if (it == m_sessions.end())
    {
        K_LOG_ERROR( "player_id(%d) TradeSession not found", player->GetId());
        return nullVector;
    }

    TradeSession* session = it->second;
    if (player == session->a_player)
        return session->b_items;
    else
        return session->a_items;
}

int TradeService::DecreaseItem(MYSQL *conn, const std::string &char_id, const TradeItem &item)
{
    long long charId = std::stoll(char_id);
    int itemId = std::stoi(item.id);
    
    if(updateInventoryItemCountMinus(conn,charId,itemId,item.amount) !=0)
    {
        K_LOG_ERROR( "updateInventoryItemCountMinus Fail");
        return -1;
    }

    if(DeleteInventoryItem(conn, charId, itemId) != 0)
    {
        K_LOG_ERROR( "DeleteInventoryItem Fail");
        return -1;
    }

    return 0;
}

int TradeService::IncreaseItem(MYSQL *conn, const std::string &char_id, TradeItem &item)
{
    bool hasItem = false;
    long long charId = std::stoll(char_id);
    int inventoryType = std::stoi(item.type);
    int itemId = std::stoi(item.id);
    int slotPos = 0;
    if(SelectInventoryItemSlot(conn,char_id,item, hasItem) != 0)
    {
        K_LOG_DEBUG( "SelectInventoryItemSlot Error");
        return -1;
    }


    if (hasItem)
    {
        int itemId = std::stoi(item.id);
        if(UpdateInventoryItemCountPlus(conn, charId, itemId, item.amount) != 0)
        {
            K_LOG_DEBUG( "UpdateInventoryItemCount fail");
            return -1;    
        }
        return 0;
    }

    if(SelectNextInventorySlotPos(conn, charId, inventoryType, slotPos) != 0)
    {
        K_LOG_DEBUG( "SelectNextInventorySlotPos fail");
        return -1;
    }

    if(InsertInventoryItem(conn, charId, inventoryType, slotPos, itemId, item.amount))
    {
        K_LOG_DEBUG( "InsertInventoryItem fail");
        return -1; 
    }
    //내 슬롯 index 업데이트
    item.slot_index = slotPos;

    return 0;
}

int TradeService::SelectInventoryItemSlot(MYSQL *conn, const std::string &char_id, TradeItem &item, bool& hasItem)
{
     int slotPos = 0;
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "SELECT FAIL: %s", mysql_error(conn));
        return -1;
    }
    // 아이템 보유 여부 확인
    const char* query = "SELECT slot_pos FROM character_inventory WHERE char_id = ? AND item_id = ? LIMIT 1"; 

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare ERROR [%s]", mysql_error(conn));
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND param[2]{};
    
    long long charId = std::stoll(char_id);

    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer = &charId;
    
    int itemId = std::stoi(item.id);
    param[1].buffer_type = MYSQL_TYPE_LONG;
    param[1].buffer =&itemId;
    
    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_execute ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND resultBind[1]{};

    resultBind[0].buffer_type = MYSQL_TYPE_LONG;
    resultBind[0].buffer = &slotPos;

    if(mysql_stmt_bind_result(stmt, resultBind) !=0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    if(mysql_stmt_store_result(stmt) !=0)
    {
        K_LOG_ERROR( "mysql_stmt_store_result ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    int fetchResult = mysql_stmt_fetch(stmt);
    if (fetchResult == 0)
    {
        hasItem = true;
    
        // 받는 사람의 실제 합쳐진 슬롯 위치
        item.slot_index = slotPos;
    
        mysql_stmt_close(stmt);
        return 0;
    }
    else if (fetchResult == MYSQL_NO_DATA)
    {
        hasItem = false; // row 없음
        mysql_stmt_close(stmt);
        return 0;
    }
    else if (fetchResult == MYSQL_DATA_TRUNCATED)
    {
        K_LOG_ERROR( "mysql_stmt_fetch DATA TRUNCATED  [%s]", mysql_stmt_error(stmt));
        hasItem = true; // 존재 여부만 보면 true
        mysql_stmt_close(stmt);
        return 0;
    }
    else
    {
        K_LOG_ERROR( "mysql_stmt_fetch ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }
}

int TradeService::UpdateInventoryItemCountPlus(MYSQL* conn, long long charId, int itemId, int amount)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init ERROR [%s]", mysql_error(conn));
        return -1;
    }

    const char* query = "UPDATE character_inventory SET item_count = item_count + ? WHERE char_id = ? AND item_id = ? ";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare ERROR [%s]", mysql_error(conn));
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND param[3]{};

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &amount;

    param[1].buffer_type = MYSQL_TYPE_LONGLONG;
    param[1].buffer = &charId;

    param[2].buffer_type = MYSQL_TYPE_LONG;
    param[2].buffer = &itemId;

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_execute ERROR [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }
    my_ulonglong affectedRows = mysql_stmt_affected_rows(stmt);

    if (affectedRows == 0)
    {
        K_LOG_ERROR( "update affected 0 rows");
        mysql_stmt_close(stmt);
        return -1;
    }
    mysql_stmt_close(stmt);
    return 0;

}

int TradeService::updateInventoryItemCountMinus(MYSQL *conn, long long charId, int itemId, int amount)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);

    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init Error [%s]", mysql_error(conn));
        return -1;
    }

    const char* query = "UPDATE character_inventory SET item_count = item_count - ? WHERE char_id = ? AND item_id = ? AND item_count >= ?";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;    
    }

    MYSQL_BIND param[4]{};

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &amount;

    param[1].buffer_type = MYSQL_TYPE_LONGLONG;
    param[1].buffer = &charId;

    param[2].buffer_type = MYSQL_TYPE_LONG;
    param[2].buffer = &itemId;

    param[3].buffer_type = MYSQL_TYPE_LONG;
    param[3].buffer = &amount;

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_execute Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    my_ulonglong affectedRows = mysql_stmt_affected_rows(stmt);

    if (affectedRows == 0)
    {
        K_LOG_ERROR( "update affected 0 rows");
        mysql_stmt_close(stmt);
        return -1;
    }
    mysql_stmt_close(stmt);

    return 0;
}

int TradeService::SelectNextInventorySlotPos(MYSQL* conn,long long charId,int inventoryType,int& slotPos)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init Error [%s]", mysql_error(conn));
        return -1;
    }
    //query = "SELECT COALESCE(MAX(slot_pos) + 1, 0) FROM character_inventory WHERE char_id = " + char_id + " AND inventory_type = " + item.type;
    const char* query = "SELECT COALESCE(MAX(slot_pos) + 1, 0) FROM character_inventory WHERE char_id = ? AND inventory_type = ?";

    if(mysql_stmt_prepare(stmt, query, strlen(query))!=0)
    {
        K_LOG_ERROR( "mysql_stmt_init Error [%s]", mysql_stmt_error(stmt));
        return -1;
    }

    MYSQL_BIND param[2]{};

    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer = &charId;

    param[1].buffer_type = MYSQL_TYPE_LONG;
    param[1].buffer = &inventoryType;

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }
    
    if(mysql_stmt_execute(stmt) !=0 )
    {
        K_LOG_ERROR( "mysql_stmt_execute Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }


    MYSQL_BIND resultBind[1]{};
    
    resultBind[0].buffer_type = MYSQL_TYPE_LONG;
    resultBind[0].buffer = &slotPos;

    if(mysql_stmt_bind_result(stmt, resultBind) !=0 )
    {
        K_LOG_ERROR( "mysql_stmt_bind_result Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    if(mysql_stmt_store_result(stmt) !=0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_result Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    int fetchResult = mysql_stmt_fetch(stmt);
    if(fetchResult == MYSQL_NO_DATA)
    {
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    if(fetchResult != 0 && fetchResult != MYSQL_DATA_TRUNCATED)
    {
        K_LOG_ERROR( "mysql_stmt_fetch ERROR [%s] Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_free_result(stmt);
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_free_result(stmt);
    mysql_stmt_close(stmt);
    return 0;
}

int TradeService::InsertInventoryItem(MYSQL* conn,long long charId,int inventoryType,int slotPos,int itemId,int amount)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init Error [%s]", mysql_error(conn));
        return -1;
    }

    const char* query = "INSERT INTO character_inventory (char_id, inventory_type, slot_pos, item_id, item_count) VALUES (?, ?, ?, ?, ?)";
    
    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND param[5]{};

    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer = &charId;

    param[1].buffer_type = MYSQL_TYPE_LONG;
    param[1].buffer = &inventoryType;

    param[2].buffer_type = MYSQL_TYPE_LONG;
    param[2].buffer = &slotPos;

    param[3].buffer_type = MYSQL_TYPE_LONG;
    param[3].buffer = &itemId;

    param[4].buffer_type = MYSQL_TYPE_LONG;
    param[4].buffer = &amount;

    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_execute Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    mysql_stmt_close(stmt);
    return 0;
}

int TradeService::DeleteInventoryItem(MYSQL *conn, long long charId, int itemId)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);

    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init Error [%s]", mysql_error(conn));
        return -1;
    }

    const char* query = "DELETE FROM character_inventory WHERE char_id = ? AND item_id = ? AND item_count <= 0";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }
    
    MYSQL_BIND param[2]{};

    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer = &charId;

    param[1].buffer_type = MYSQL_TYPE_LONG;
    param[1].buffer = &itemId;


    if(mysql_stmt_bind_param(stmt, param) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_bind_param Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    if(mysql_stmt_execute(stmt) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_execute Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    my_ulonglong affectedRows = mysql_stmt_affected_rows(stmt);

    if (affectedRows == 0)
    {
        mysql_stmt_close(stmt);
        return 0;
    }
    mysql_stmt_close(stmt);
    return 0;
}
