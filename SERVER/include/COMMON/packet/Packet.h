#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include <string>

class Client;

class RedisClient;
class MySqlconnectionPool;
class RedisConnectionPool;

class WorldServer;
class WorldSession;
class CharacterService;
class ChannelManager;

class ChannelServer;
class ChannelSession;
class PlayerManager;
class PlayerService;
class MapService;
class StatService;
class ItemManager;
class ItemService;
class CombatService;
class TradeService;

class CommandDispatcher;


#define BUFFER_SIZE 1024

#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t length; // 전체 패킷 길이
    uint16_t type;   // 패킷 타입
};
#pragma pack(pop)

// 패킷타입 정의
enum PACKET_TYPE : uint16_t {
     // 0x0001 ~ 0x001F : 로그인 / 월드
    PKT_LOGIN               = 0x0001,
    PKT_REGISTER            = 0x0002,
    PKT_CHAT_INIT           = 0x0003,
    PKT_CHAT                = 0x0004,
    PKT_INIT_WORLD          = 0x0005,
    PKT_SELECT_CHARACTER    = 0x0006,
    PKT_SELECT_CHANNEL      = 0x0007,
    PKT_INIT_CHANNEL        = 0x0008,
    PKT_CHANNEL_AUTH        = 0x0009,
    PKT_ENTER_MAP           = 0x000A,

    // 0x0020 ~ 0x003F : 플레이어
    PKT_PLAYER_MOVE         = 0x0020,
    PKT_PLAYER_ATTACK       = 0x0021,
    PKT_PLAYER_ONDAMAGED    = 0x0022,
    PKT_PLAYER_USE_ITEM     = 0x0023,
    PKT_PLAYER_INFO         = 0x0024,
    PKT_PLAYER_STAT         = 0x0025,
    PKT_PLAYER_SKILLLIST    = 0x0026,
    PKT_PLAYER_BASIC_ATTACK = 0x0027,
    PKI_PLAYER_EXP_GAIN     = 0x0028,
    PKT_PLAYER_PICKUP_ITEM  = 0x0029,
    PKT_OTHERPLAYER_ENTER   = 0x0030,
    PKT_OTHERPLAYER_SNAPSHOT = 0x0031,
    PKT_OTHER_PLAYER_ATTACK = 0x0032,

    // 0x0040 ~ 0x005F : 몬스터
    PKT_MONSTER_MOVE        = 0x0040,
    PKT_MONSTER_ONDAMAGED   = 0x0041,
    PKT_MONSTER_SNAPSHOT    = 0x0042,
    PKT_MONSTER_RESPAWN     = 0x0043,

    // 0x0060 ~ 0x007F : 드롭
    PKT_DROPITEMS           = 0x0060,
    PKT_REMOVEITEMS         = 0x0061,

    // 0x0080 ~ 0x009F : 인벤토리
    PKT_INVENTORY_META_INFO = 0x0080,
    PKT_INVENTORY_ITEM_INFO = 0x0081,
    PKT_INVENTORY_ITEM_MOVE = 0x0082,

    // 0x0100 ~ 0x010F : 교환
    PKT_TRADE_REQUEST        = 0x0100,  // 교환 신청
    PKT_TRADE_ACCEPT         = 0x0101,  // 교환 수락
    PKT_TRADE_START          = 0x0102,  // 교환 실행
    PKT_TRADE_READY          = 0x0103,  // 교환 준비(교환하기 버튼 누름)
    PKT_TRADE_CONFIRM        = 0x0104,  // 교환 성사
    PKT_TRADE_CANCEL         = 0x0105,  // 교환 취소
    PKT_TRADE_ADD_ITEM       = 0x0106,  // 아이템 추가

    // 0x1000 ~ : 테스트 / UI / 특수
    PKT_STAT_VIEW           = 0x1000,
    PKT_STAT_UP             = 0x1001,
    PKT_QUICKSLOT_LIST      = 0x1002,
    PKT_QUICKSLOT_SET       = 0x1003,
};

struct ParsedPacket
{
    uint16_t type;
    std::string payload;
};

struct PacketContext
{
    Client* client = nullptr;
    uint16_t type;
    int fd = -1;
    char *payload = nullptr;
    int payload_len = 0;

    // Chat에서 사용
    std::vector<Client*>* clients = nullptr;
    std::function<void(const std::string&, const std::string&, int)>broadcast;
    CommandDispatcher* dispatcher = nullptr;

    //World
    WorldSession *world_session = nullptr;
    CharacterService *char_service = nullptr;
    ChannelManager *channel_manager = nullptr;
    RedisConnectionPool* redis_pool = nullptr;
    
    //Channel에서 사용
    ChannelSession *channel_session = nullptr;
    PlayerService* player_service = nullptr;
    PlayerManager* player_manager =nullptr;
    MapService* map_service = nullptr;
    StatService* stat_service = nullptr;
    ItemManager* item_manager =nullptr;
    ItemService* item_service = nullptr;
    CombatService* combat_service = nullptr;
    TradeService* trade_service = nullptr;
};

typedef struct packet
{
    PacketHeader header{};
    std::vector<uint8_t> payload;
} Packet;

enum class ParseStatus
{
    Complete,
    NeedMoreData,
    InvalidPacket,
};

struct ParseResult
{
    ParseStatus status;
    ParsedPacket packet;
};

