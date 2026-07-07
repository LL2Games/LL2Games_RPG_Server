#pragma once
#include "Player.h"
#include "MySqlConnectionPool.h"
//#include "RedisClient.h"
#include <vector>
#include <unordered_map>
#include <mutex>
struct TradeItem
{
    std::string id;
    std::string type;
    size_t amount;
    size_t slot_index;
};
struct TradeSession
{
    Player* a_player;
    Player* b_player;

    int a_id;
    int b_id;

    bool a_ready;
    bool b_ready;

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
    int Execute(TradeSession *);
    int Execute(TradeExecuteData& data);
    int DecreaseItem(MYSQL *conn, const std::string &char_id, const TradeItem &item);
    int IncreaseItem(MYSQL *conn, const std::string &char_id, TradeItem &item);

    int SelectInventoryItemSlot(MYSQL *conn, const std::string &char_id, TradeItem &item, bool& hasItem);
    int UpdateInventoryItemCountPlus(MYSQL* conn, long long charId, int itemId, int amount);
    int updateInventoryItemCountMinus(MYSQL* conn, long long charId, int itemId, int amount);
    int SelectNextInventorySlotPos(MYSQL* conn,long long charId,int inventoryType,int& slotPos);
    int InsertInventoryItem(MYSQL* conn,long long charId,int inventoryType,int slotPos,int itemId,int amount);
    int DeleteInventoryItem(MYSQL* conn,long long charId,int itemId);

public:
    TradeService();
    ~TradeService();

    int Request(Player* requester, Player* target_player, std::string &errMsg);
    int Start(Player* requester, Player* accepter, std::string &errMsg);
    int UploadItem(Player*, const TradeItem&, std::string &errMsg);
    int Ready(Player*, const std::vector<TradeItem>&, std::string &errMsg);
    int Cancel(Player* requester, std::string &errMsg);

private:
    MySqlConnectionPool* m_mySql;
    //RedisClient* m_redis;

    static std::mutex m_TradeMutex;
};