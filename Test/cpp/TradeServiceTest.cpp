#include "TradeService.h"
#include "InventoryManager.h"
#include "ItemManager.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

class TradeServiceRegressionTest
{
public:
    static bool CreateSession(TradeService& service,Player* first,Player* second)
    {
        service.CreateTradeSession(first, second);

        std::lock_guard<std::mutex> lock(TradeService::m_TradeMutex);
        auto it = TradeService::m_sessions.find(first->GetId());

        return it != TradeService::m_sessions.end() &&it->second != nullptr;
    }

    static void DeleteSession(const int playerId)
    {
        std::lock_guard<std::mutex> lock(TradeService::m_TradeMutex);

        auto it = TradeService::m_sessions.find(playerId);
        if (it == TradeService::m_sessions.end() ||it->second == nullptr)
        {
            return;
        }

        TradeSession* session = it->second;

        TradeService::m_sessions.erase(session->a_id);
        TradeService::m_sessions.erase(session->b_id);

        delete session;
    }
};

namespace
{
    constexpr int kFirstPlayerId = 910001;
    constexpr int kSecondPlayerId = 910002;
    constexpr int kInventoryType = inven::Consume;
    constexpr int kInventorySlot = 3;
    constexpr int kItemId = 2000000;
    constexpr int kOwnedAmount = 10;

    bool Check(const bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "[FAIL] " << message << '\n';
            return false;
        }

        std::cout << "[PASS] " << message << '\n';
        return true;
    }

    bool InitializeInventory(Player& player)
    {
        InventoryManager* manager = player.GetInventoryManager();

        if (manager == nullptr)
            return false;

        InventoryMetaInfo metaInfo{};
        metaInfo.inventoryType = kInventoryType;
        metaInfo.max_slots = 32;
        metaInfo.currnet_slots_size = 0;

        if (!manager->CreateInventory(metaInfo))
            return false;

        Inventory* inventory = manager->GetInventory(kInventoryType);

        if (inventory == nullptr)
            return false;

        return inventory->SetSlotItem(kInventorySlot,kItemId,kOwnedAmount);
    }

    void InitializePlayer(Player& player, const int id)
    {
        player.SetId(id);
        InitializeInventory(player);
    }

    struct TradeFixture
    {
        Player first;
        Player second;
        TradeService service;

        TradeFixture()
        {
            InitializePlayer(first, kFirstPlayerId);
            InitializePlayer(second, kSecondPlayerId);

            TradeServiceRegressionTest::CreateSession(service, &first,&second);
        }

        ~TradeFixture()
        {
            TradeServiceRegressionTest::DeleteSession(kFirstPlayerId);
        }
    };

    TradeItem MakeItem(const int amount,const int slot = kInventorySlot,const int itemId = kItemId)
    {
        TradeItem item{};
        item.id = std::to_string(itemId);

        // UploadItem이 클라이언트 type을 신뢰하지 않는지도 확인한다.
        item.type = "999";
        item.amount = amount;
        item.slot_index = slot;

        return item;
    }

    bool TestZeroAmountRejected()
    {
        TradeFixture fixture;
        std::string errMsg;

        const int result = fixture.service.UploadItem(&fixture.first,MakeItem(0),errMsg);

        return Check(result != 0,"교환 수량 0 거절");
    }

    bool TestNegativeAmountRejected()
    {
        TradeFixture fixture;
        std::string errMsg;

        const int result = fixture.service.UploadItem(&fixture.first,MakeItem(-1),errMsg);

        return Check(result != 0,"음수 교환 수량 거절");
    }

    bool TestUnknownSlotRejected()
    {
        TradeFixture fixture;
        std::string errMsg;

        const int result = fixture.service.UploadItem(&fixture.first,MakeItem(1, 99),errMsg);

        return Check(result != 0,"존재하지 않는 인벤토리 슬롯 거절");
    }

    bool TestMismatchedItemRejected()
    {
        TradeFixture fixture;
        std::string errMsg;

        // 슬롯 3에는 2000000이 있지만 2000001을 등록한다.
        const int result = fixture.service.UploadItem(&fixture.first,MakeItem(1, kInventorySlot, 2000001),errMsg);
        return Check(result != 0,"슬롯과 일치하지 않는 아이템 ID 거절");
    }

    bool TestExcessAmountRejected()
    {
        TradeFixture fixture;
        std::string errMsg;

        const int result = fixture.service.UploadItem(&fixture.first,MakeItem(kOwnedAmount + 1),errMsg);
        return Check(result != 0,"보유 수량 초과 등록 거절");
    }

    bool TestDuplicateAccumulationRejected()
    {
        TradeFixture fixture;
        std::string errMsg;

        const int firstResult =fixture.service.UploadItem(&fixture.first,MakeItem(6),errMsg);

        errMsg.clear();

        const int secondResult = fixture.service.UploadItem(&fixture.first,MakeItem(5),errMsg);

        return
            Check(firstResult == 0, "동일 슬롯 첫 번째 등록 성공") &&
            Check(secondResult != 0, "동일 슬롯 누적 보유량 초과 거절");
    }

    bool TestValidItemAccepted()
    {
        TradeFixture fixture;
        std::string errMsg;

        const int result = fixture.service.UploadItem(&fixture.first,MakeItem(5),errMsg);

        return Check(result == 0,"정상 교환 아이템 등록 성공");
    }
}

int main()
{
    // ItemManager의 상대 경로가 SERVER/bin 기준으로 동작한다.
    const std::filesystem::path executablePath = std::filesystem::read_symlink("/proc/self/exe");

    std::filesystem::current_path(executablePath.parent_path());

    ItemManager* itemManager = ItemManager::GetInstance();

    if (!Check(itemManager != nullptr && itemManager->Init(),"테스트 아이템 데이터 로딩"))
    {
        return EXIT_FAILURE;
    }

    if (!TestZeroAmountRejected() ||
        !TestNegativeAmountRejected() ||
        !TestUnknownSlotRejected() ||
        !TestMismatchedItemRejected() ||
        !TestExcessAmountRejected() ||
        !TestDuplicateAccumulationRejected() ||
        !TestValidItemAccepted())
    {
        return EXIT_FAILURE;
    }

    std::cout
        << "교환 아이템 검증 회귀 테스트 전체 통과\n";

    return EXIT_SUCCESS;
}