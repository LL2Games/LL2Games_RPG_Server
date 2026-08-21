#pragma once
#include "Player.h"
#include "MySqlConnectionPool.h"
//#include "RedisClient.h"
#include <vector>
#include <unordered_map>
#include <mutex>

class TradeServiceRegressionTest;

struct TradeItem
{
    std::string id;
    std::string type;
    //size_t에서 int로 변경
    // 클라이언트가 -1을 보내게 되는 경우 매우 큰 양수로 변활 가능성
    // 및 DB 함수에서 다시 int로 축소되어 통일
    int amount = 0;
    int slot_index = -1;
};
struct TradeSession
{
    Player* a_player = nullptr;
    Player* b_player = nullptr;

    int a_id = -1;
    int b_id = -1;

    bool a_ready = false;
    bool b_ready = false;
    bool executing = false;

    std::vector<TradeItem> a_items;
    std::vector<TradeItem> b_items;
};

struct TradeExecuteData
{
    int a_id;
    int b_id;
    std::vector<TradeItem> a_items;
    std::vector<TradeItem> b_items;
};
class TradeService
{
private:
    static std::unordered_map<int, TradeSession*> m_sessions; //<player_id, session*>
    void CreateTradeSession(Player *, Player *);

public:
    TradeSession* GetTradeSession(Player *);
    Player* GetTargetPlayer(Player *);
    void DeleteTradeSession(TradeSession*);
    const std::vector<TradeItem>& GetMyItems(Player *);
    const std::vector<TradeItem>& GetTargetItems(Player *);

private:
   
    int Execute(TradeExecuteData& data);
    int DecreaseItem(MYSQL *conn, const std::string &char_id, const TradeItem &item);
    int IncreaseItem(MYSQL *conn, const std::string &char_id, TradeItem &item);

    int SelectInventoryItemSlot(MYSQL *conn, const std::string &char_id, TradeItem &item, bool& hasItem);
    int UpdateInventoryItemCountPlus(MYSQL* conn, long long charId, int inventoryType, int slotPos, int itemId, int amount);
    int updateInventoryItemCountMinus(MYSQL* conn, long long charId, int inventoryType, int slotPos, int itemId, int amount);
    int SelectNextInventorySlotPos(MYSQL* conn,long long charId,int inventoryType,int& slotPos);
    int InsertInventoryItem(MYSQL* conn,long long charId,int inventoryType,int slotPos,int itemId,int amount);
    int DeleteInventoryItem(MYSQL* conn,long long charId,int inventoryType, int slotPos, int itemId);

    // 준비 버튼을 누르기 전에 아이템 수량이 변경될s 수 있어 확인 함수 추가
    bool ValidateTradeItems(Player* player, const std::vector<TradeItem>& items, std::string& errMsg) const;
public:
    TradeService();
    ~TradeService();

    int Request(Player* requester, Player* target_player, std::string &errMsg);
    int Start(Player* requester, Player* accepter, std::string &errMsg);
    int UploadItem(Player*, const TradeItem&, std::string &errMsg);
    int Ready(Player*, const std::vector<TradeItem>&, std::string &errMsg);
    int Cancel(Player* requester, std::string &errMsg);

private:
    friend class TradeServiceRegressionTest;

    MySqlConnectionPool* m_mySql;
    //RedisClient* m_redis;

    static std::mutex m_TradeMutex;
};