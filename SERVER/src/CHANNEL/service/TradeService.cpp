#include "TradeService.h"
//#include "TradeRequestTask.h"
#include "MapInstance.h"
#include "MySqlConnectionPool.h"
//#include "RedisClient.h"
#include "ItemManager.h"
#include "Inventory_Info.h"
#include "K_slog.h"

#include <limits>
#include <limits>
#include <map>
#include <tuple>

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

    int itemId = 0;

    try
    {
        size_t parsedLength = 0;
        itemId = std::stoi(item.id, &parsedLength);

        if (parsedLength != item.id.size())
        {
            errMsg = "Invalid item id";
            return -1;
        }
    }
    catch (const std::exception&)
    {
        errMsg = "Invalid item id";
        return -1;
    }

    if (itemId <= 0 || item.amount <= 0 || item.slot_index < 0)
    {
        errMsg = "Invalid trade item";
        return -1;
    }

    ItemManager* itemManager = ItemManager::GetInstance();
    if (itemManager == nullptr)
    {
        errMsg = "Item manager is unavailable";
        return -1;
    }

    const auto* itemData = itemManager->Find(itemId);
    if (itemData == nullptr)
    {
        errMsg = "Item does not exist";
        return -1;
    }

    // 클라이언트가 계산한 type을 신뢰하지 않고
    // 서버의 아이템 데이터에서 인벤토리 타입을 결정한다.
    const int inventoryType = inven::ConvertItemTypeToInventoryType(itemData->type);

    InventoryManager* inventoryManager = player->GetInventoryManager();
    if (inventoryManager == nullptr)
    {
        errMsg = "Inventory manager is unavailable";
        return -1;
    }

    std::lock_guard<std::mutex> lock(m_TradeMutex);

    auto sessionIt = m_sessions.find(player->GetId());
    if (sessionIt == m_sessions.end() || sessionIt->second == nullptr)
    {
        errMsg = "Player trade session not found";
        return -1;
    }

    TradeSession* session = sessionIt->second;

    std::vector<TradeItem>* tradeItems = nullptr;

    if (session->a_id == player->GetId())
    {
        tradeItems = &session->a_items;
    }
    else if (session->b_id == player->GetId())
    {
        tradeItems = &session->b_items;
    }
    else
    {
        errMsg = "Player does not belong to this trade";
        return -1;
    }

    if (session->a_ready || session->b_ready || session->executing)
    {
        errMsg = "Trade items cannot be changed after ready";
        return -1;
    }

    long long totalOfferedAmount = item.amount;

    for (const TradeItem& uploadedItem : *tradeItems)
    {
        if (uploadedItem.slot_index != item.slot_index)
            continue;

        int uploadedItemId = 0;

        try
        {
            uploadedItemId = std::stoi(uploadedItem.id);
        }
        catch (const std::exception&)
        {
            errMsg = "Invalid item in trade session";
            return -1;
        }

        // 동일한 인벤토리 슬롯이 다른 아이템 ID로 등록된 상태
        if (uploadedItemId != itemId)
        {
            errMsg = "Trade slot item mismatch";
            return -1;
        }

        totalOfferedAmount += uploadedItem.amount;

        if (totalOfferedAmount > std::numeric_limits<int>::max())
        {
            errMsg = "Trade item amount is too large";
            return -1;
        }
    }

    // 같은 슬롯을 여러 번 등록했다면 합산 수량으로 검사한다.
    if (!inventoryManager->HasItemBySlot(
            inventoryType,
            item.slot_index,
            itemId,
            static_cast<int>(totalOfferedAmount)))
    {
        errMsg = "Item ownership or amount validation failed";
        return -1;
    }

    TradeItem validatedItem = item;

    // 패킷에서 계산한 타입 대신 서버 데이터 기준 타입을 저장한다.
    validatedItem.id = std::to_string(itemId);
    validatedItem.type = std::to_string(inventoryType);

    tradeItems->push_back(validatedItem);

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
    TradeSession* executingSession = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_TradeMutex);

        auto it = m_sessions.find(player->GetId());
        if (it == m_sessions.end() || it->second == nullptr)
        {
            errMsg = "Player trade session not found";
            return -1;
        }

        TradeSession* session = it->second;

        if (session->executing)
        {
            errMsg = "Trade is already executing";
            return -1;
        }

        if (session->a_id == player->GetId())
        {
            session->a_ready = true;
        }
        else if (session->b_id == player->GetId())
        {
            session->b_ready = true;
        }
        else
        {
            errMsg = "Player does not belong to this trade";
            return -1;
        }

        // 상대방이 아직 준비하지 않은 상태
        if (!session->a_ready || !session->b_ready)
            return 2;

        // 실행 직전에 양쪽 인벤토리를 다시 확인한다.
        if (!ValidateTradeItems(session->a_player, session->a_items, errMsg) ||
            !ValidateTradeItems(session->b_player, session->b_items, errMsg))
        {
            m_sessions.erase(session->a_id);
            m_sessions.erase(session->b_id);

            delete session;
            return -1;
        }

        // mutex를 풀기 전에 실행 상태를 설정해야 한다.
        session->executing = true;
        executingSession = session;

        executeData.a_id = session->a_id;
        executeData.b_id = session->b_id;
        executeData.a_items = session->a_items;
        executeData.b_items = session->b_items;
    }

    const int result = Execute(executeData);

    if (result != 0)
    {
        std::lock_guard<std::mutex> lock(m_TradeMutex);

        auto it = m_sessions.find(player->GetId());

        if (it != m_sessions.end() && it->second == executingSession)
        {
            m_sessions.erase(executingSession->a_id);
            m_sessions.erase(executingSession->b_id);

            delete executingSession;
        }

        errMsg = "Trade transaction failed";
        return -1;
    }

    // 성공 시 executing은 유지한다.
    // HandleTradeReady가 결과 패킷을 만든 후 세션을 제거한다.
    return 0;
}


int TradeService::Execute(TradeExecuteData& data)
{
    MYSQL* conn = m_mySql->GetConnection();

    if (conn == nullptr)
    {
        K_LOG_ERROR( "MYSQL GetConnection failed");
        return -1;
    }

    int result = 0;

    if (mysql_query(conn, "START TRANSACTION") != 0)
    {
        K_LOG_ERROR( "START TRANSACTION failed");
        // 연결 장애일 가능성이 있으므로 풀에 반환하지 않는다.
        mysql_close(conn);
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
        {
            K_LOG_ERROR("COMMIT failed: %s",mysql_error(conn));
            result = -1;
            if (mysql_query(conn, "ROLLBACK") != 0)
            {
                K_LOG_ERROR("ROLLBACK after COMMIT failure failed: %s",mysql_error(conn));
                // 상태를 보장할 수 없는 연결은 풀에 반환하지 않는다.
                mysql_close(conn);
                return -1;
            }
        }
    }
    else
    {
        if (mysql_query(conn, "ROLLBACK") != 0)
        {
            K_LOG_ERROR("ROLLBACK failed: %s", mysql_error(conn));
            // 실패한 연결을 풀에 다시 넣지 않는다.
            mysql_close(conn);
            return -1;
        }

    K_LOG_DEBUG("Trade transaction rolled back");
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

    if (session->executing)
    {
        errMsg = "Trade is already executing";
        return -1;
    }


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
    long long charId = 0;
    int itemId = 0;
    int inventoryType = 0;

    try
    {
        charId = std::stoll(char_id);
        itemId = std::stoi(item.id);
        inventoryType = std::stoi(item.type);
    }
    catch (const std::exception&)
    {
        K_LOG_ERROR("Invalid trade item DB parameter");
        return -1;
    }

    const int slotPos = item.slot_index;
    const int amount = item.amount;

    if (charId <= 0 || itemId <= 0 || inventoryType < 0 || slotPos < 0 || amount <= 0)
    {
        K_LOG_ERROR("Trade item DB parameter is out of range");
        return -1;
    }

    if (updateInventoryItemCountMinus(conn, charId, inventoryType, slotPos, itemId, amount) != 0)
    {
        K_LOG_ERROR("updateInventoryItemCountMinus failed");
        return -1;
    }

    if (DeleteInventoryItem(conn, charId, inventoryType, slotPos, itemId) != 0)
    {
        K_LOG_ERROR("DeleteInventoryItem failed");
        return -1;
    }

    return 0;
}

int TradeService::IncreaseItem(MYSQL *conn, const std::string &char_id, TradeItem &item)
{
     long long charId = 0;
    int inventoryType = 0;
    int itemId = 0;

    try
    {
        charId = std::stoll(char_id);
        inventoryType = std::stoi(item.type);
        itemId = std::stoi(item.id);
    }
    catch (const std::exception&)
    {
        K_LOG_ERROR("Invalid trade item DB parameter");
        return -1;
    }

    if (charId <= 0 || inventoryType < 0 || itemId <= 0 || item.amount <= 0)
    {
        K_LOG_ERROR("Trade item DB parameter is out of range");
        return -1;
    }

    bool hasItem = false;

    if (SelectInventoryItemSlot(conn, char_id, item, hasItem) != 0)
    {
        K_LOG_ERROR("SelectInventoryItemSlot failed");
        return -1;
    }

    if (hasItem)
    {
        if (UpdateInventoryItemCountPlus(conn, charId, inventoryType, item.slot_index, itemId, item.amount) != 0)
        {
            K_LOG_ERROR("UpdateInventoryItemCountPlus failed");
            return -1;
        }

        return 0;
    }

    int slotPos = 0;

    if (SelectNextInventorySlotPos(conn, charId, inventoryType, slotPos) != 0)
    {
        K_LOG_ERROR("SelectNextInventorySlotPos failed");
        return -1;
    }

    if (InsertInventoryItem(conn, charId, inventoryType, slotPos, itemId, item.amount) != 0)
    {
        K_LOG_ERROR("InsertInventoryItem failed");
        return -1;
    }

    // 받는 사람에게 전송할 실제 슬롯
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
    const char* query = "SELECT slot_pos FROM character_inventory WHERE char_id = ? AND inventory_type = ?  AND item_id = ? ORDER BY slot_pos ASC LIMIT 1"; 

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare ERROR [%s]", mysql_error(conn));
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND param[3]{};
    
    long long charId = std::stoll(char_id);
    int inventoryType = std::stoi(item.type);
    int itemId = std::stoi(item.id);

    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer = &charId;

    param[1].buffer_type = MYSQL_TYPE_LONG;
    param[1].buffer = &inventoryType;

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

int TradeService::UpdateInventoryItemCountPlus(MYSQL* conn, long long charId, int inventoryType, int slotPos, int itemId, int amount)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init ERROR [%s]", mysql_error(conn));
        return -1;
    }

    const char* query = "UPDATE character_inventory SET item_count = item_count + ? WHERE char_id = ? AND inventory_type = ? AND slot_pos = ? AND item_id = ? ";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare ERROR [%s]", mysql_error(conn));
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND param[5]{};

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &amount;

    param[1].buffer_type = MYSQL_TYPE_LONGLONG;
    param[1].buffer = &charId;

    param[2].buffer_type = MYSQL_TYPE_LONG;
    param[2].buffer = &inventoryType;

    param[3].buffer_type = MYSQL_TYPE_LONG;
    param[3].buffer = &slotPos;

    param[4].buffer_type = MYSQL_TYPE_LONG;
    param[4].buffer = &itemId;

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
    const my_ulonglong affectedRows = mysql_stmt_affected_rows(stmt);

    mysql_stmt_close(stmt);

    if (affectedRows != 1)
    {
        K_LOG_ERROR("Inventory increase failed. affectedRows[%llu]", static_cast<unsigned long long>(affectedRows));
        return -1;
    }
  
    return 0;

}

int TradeService::updateInventoryItemCountMinus(MYSQL *conn, long long charId, int inventoryType, int slotPos,int itemId, int amount)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);

    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init Error [%s]", mysql_error(conn));
        return -1;
    }

    const char* query = "UPDATE character_inventory SET item_count = item_count - ? WHERE char_id = ? AND inventory_type = ? AND slot_pos = ? AND item_id = ? AND item_count >= ?";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;    
    }

    MYSQL_BIND param[6]{};

    param[0].buffer_type = MYSQL_TYPE_LONG;
    param[0].buffer = &amount;

    param[1].buffer_type = MYSQL_TYPE_LONGLONG;
    param[1].buffer = &charId;

    param[2].buffer_type = MYSQL_TYPE_LONG;
    param[2].buffer = &inventoryType;

    param[3].buffer_type = MYSQL_TYPE_LONG;
    param[3].buffer = &slotPos;

    param[4].buffer_type = MYSQL_TYPE_LONG;
    param[4].buffer = &itemId;

    param[5].buffer_type = MYSQL_TYPE_LONG;
    param[5].buffer = &amount;

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

    const my_ulonglong affectedRows = mysql_stmt_affected_rows(stmt);

    mysql_stmt_close(stmt);

    // 슬롯·아이템 불일치 또는 수량 부족
    if (affectedRows != 1)
    {
        K_LOG_ERROR(
            "Inventory decrease failed. affectedRows[%llu]",
            static_cast<unsigned long long>(affectedRows));

        return -1;
    }

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

int TradeService::DeleteInventoryItem(MYSQL *conn, long long charId, int inventoryType, int slotPos, int itemId)
{
    MYSQL_STMT* stmt = mysql_stmt_init(conn);

    if(!stmt)
    {
        K_LOG_ERROR( "mysql_stmt_init Error [%s]", mysql_error(conn));
        return -1;
    }

    const char* query = "DELETE FROM character_inventory WHERE char_id = ? AND inventory_type = ?  AND slot_pos = ? AND item_id = ? AND item_count <= 0";

    if(mysql_stmt_prepare(stmt, query, strlen(query)) != 0)
    {
        K_LOG_ERROR( "mysql_stmt_prepare Error [%s]", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }
    
    MYSQL_BIND param[4]{};

    param[0].buffer_type = MYSQL_TYPE_LONGLONG;
    param[0].buffer = &charId;

    param[1].buffer_type = MYSQL_TYPE_LONG;
    param[1].buffer = &inventoryType;

    param[2].buffer_type = MYSQL_TYPE_LONG;
    param[2].buffer = &slotPos;

    param[3].buffer_type = MYSQL_TYPE_LONG;
    param[3].buffer = &itemId;


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

bool TradeService::ValidateTradeItems(Player* player, const std::vector<TradeItem>& items, std::string& errMsg) const
{
    if (player == nullptr)
    {
        errMsg = "Invalid player";
        return false;
    }

    ItemManager* itemManager = ItemManager::GetInstance();
    InventoryManager* inventoryManager = player->GetInventoryManager();

    if (itemManager == nullptr || inventoryManager == nullptr)
    {
        errMsg = "Inventory service is unavailable";
        return false;
    }

    // inventoryType, slotIndex, itemId별 누적 교환 수량
    std::map<std::tuple<int, int, int>, long long> offeredAmounts;

    for (const TradeItem& item : items)
    {
        int itemId = 0;

        try
        {
            size_t parsedLength = 0;
            itemId = std::stoi(item.id, &parsedLength);

            if (parsedLength != item.id.size())
            {
                errMsg = "Invalid trade item id";
                return false;
            }
        }
        catch (const std::exception&)
        {
            errMsg = "Invalid trade item id";
            return false;
        }

        if (itemId <= 0 || item.amount <= 0 || item.slot_index < 0)
        {
            errMsg = "Invalid trade item";
            return false;
        }

        const auto* itemData = itemManager->Find(itemId);
        if (itemData == nullptr)
        {
            errMsg = "Trade item does not exist";
            return false;
        }

        const int inventoryType = inven::ConvertItemTypeToInventoryType(itemData->type);
        const auto key = std::make_tuple(inventoryType, item.slot_index, itemId);

        long long& totalAmount = offeredAmounts[key];
        totalAmount += item.amount;

        if (totalAmount > std::numeric_limits<int>::max())
        {
            errMsg = "Trade item amount is too large";
            return false;
        }
    }

    for (const auto& entry : offeredAmounts)
    {
        const auto& key = entry.first;
        const long long totalAmount = entry.second;

        const int inventoryType = std::get<0>(key);
        const int slotIndex = std::get<1>(key);
        const int itemId = std::get<2>(key);

        if (!inventoryManager->HasItemBySlot(
                inventoryType,
                slotIndex,
                itemId,
                static_cast<int>(totalAmount)))
        {
            errMsg = "Trade item ownership changed before execution";
            return false;
        }
    }

    return true;
}