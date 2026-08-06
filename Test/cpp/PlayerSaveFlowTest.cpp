#include "Player.h"
#include "PlayerDataSaveService.h"

#include <string>
#include <cstdlib>
#include <iostream>

namespace
{
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

    void InitializePlayer(Player& player)
    {
        const PlayerInitData initData{
            1,
            "save-flow-test",
            "save-flow-player",
            1,
            0,
            20000,
            100000000,
            0.0F,
            0.0F
        };

        const CharacterStat stat{
            BaseStat{4, 4, 4, 4},
            DerivedStat{100, 50},
            ExpStat{1, 0, 100},
            100,
            50,
            0
        };

        player.SetInitData(initData, stat);
    }

bool TestSaveServiceFailureFlow()
{
    Player player;
    InitializePlayer(player);

    // 이 테스트 프로세스에서는 MySQLConnectionPool을 초기화하지 않는다.
    // 따라서 실제 저장을 시도하면 Repository에서 실패해야 한다.
    PlayerDataSaveService saveService;

    std::string errMsg;

    // 변경되지 않은 플레이어는 Repository를 호출하지 않아야 한다.
    const bool unchangedSaveResult = saveService.SaveIfNeeded(player, errMsg);

    if (!Check(unchangedSaveResult,"변경 없는 플레이어의 저장소 호출 생략"))
    {
        return false;
    }

    if (!Check(errMsg.empty(),"저장 생략 시 오류 미발생"))
    {
        return false;
    }

    // 변경 상태를 만든다.
    player.MarkSaveNeeded();

    const bool failedSaveResult =saveService.SaveIfNeeded(player, errMsg);

    if (!Check(!failedSaveResult,"MySQL 풀 미초기화 상태에서 저장 실패"))
    {
        return false;
    }

    if (!Check(errMsg == "MySQL connection pool is not initialized","저장 실패 사유 반환 확인"))
    {
        return false;
    }

    // 저장 실패 후에는 dirty 버전을 지우면 안 된다.
    if (!Check(player.IsSaveNeeded(),"서비스 저장 실패 후 대기 상태 유지"))
    {
        return false;
    }

    return true;
}
}

int main()
{
    Player player;
    InitializePlayer(player);

    // 초기 상태에서는 저장이 필요하지 않다.
    if (!Check(!player.IsSaveNeeded(),"변경 없는 플레이어는 저장 불필요"))
    {
        return EXIT_FAILURE;
    }

    // 상태가 변경되면 저장이 필요하다.
    player.MarkSaveNeeded();

    if (!Check(player.IsSaveNeeded(),"변경된 플레이어는 저장 필요"))
    {
        return EXIT_FAILURE;
    }

    const PlayerSaveData firstSaveData = player.MakeSaveData();

    if (!Check(firstSaveData.saveVersion == 1, "저장 스냅샷에 현재 버전 기록"))
    {
        return EXIT_FAILURE;
    }

    // 저장 중 추가 변경이 없으면 저장 완료 처리에 성공한다.
    if (!Check(player.TryMarkSaved(firstSaveData.saveVersion),"일치하는 저장 버전으로 저장 대기 상태 해제"))
    {
        return EXIT_FAILURE;
    }

    if (!Check(!player.IsSaveNeeded(),"저장 성공 후 대기 상태 해제"))
    {
        return EXIT_FAILURE;
    }

    // 저장용 스냅샷을 생성한다.
    player.MarkSaveNeeded();
    const PlayerSaveData staleSaveData = player.MakeSaveData();

    // DB 저장이 진행되는 동안 새로운 변경이 발생한 상황을 만든다.
    player.MarkSaveNeeded();

    if (!Check(!player.TryMarkSaved(staleSaveData.saveVersion),"오래된 저장 버전 거부"))
    {
        return EXIT_FAILURE;
    }

    if (!Check(player.IsSaveNeeded(),"오래된 저장 완료 후 신규 변경 상태 유지"))
    {
        return EXIT_FAILURE;
    }

    const PlayerSaveData latestSaveData = player.MakeSaveData();

    if (!Check(latestSaveData.saveVersion > staleSaveData.saveVersion,"최신 스냅샷의 저장 버전 증가 확인"))
    {
        return EXIT_FAILURE;
    }

    if (!Check(player.TryMarkSaved(latestSaveData.saveVersion),"최신 저장 버전 승인"))
    {
        return EXIT_FAILURE;
    }

    if (!Check(!player.IsSaveNeeded(),"최신 저장 성공 후 대기 상태 해제"))
    {
        return EXIT_FAILURE;
    }

    // 저장 실패 상황에서는 TryMarkSaved를 호출하지 않는다.
    player.MarkSaveNeeded();
    const PlayerSaveData failedSaveData = player.MakeSaveData();

    if (!Check(player.IsSaveNeeded(),"저장 실패 후 대기 상태 유지"))
    {
        return EXIT_FAILURE;
    }

    // 다음 주기에 같은 변경 버전으로 재시도할 수 있어야 한다.
    const PlayerSaveData retrySaveData = player.MakeSaveData();

    if (!Check(retrySaveData.saveVersion == failedSaveData.saveVersion, "실패한 저장 버전의 재시도 가능 상태 유지"))
    {
        return EXIT_FAILURE;
    }

    if (!Check(player.TryMarkSaved(retrySaveData.saveVersion),"재시도 저장 성공"))
    {
        return EXIT_FAILURE;
    }

    if (!Check(!player.IsSaveNeeded(),"재시도 성공 후 대기 상태 해제"))
    {
        return EXIT_FAILURE;
    }

    if (!TestSaveServiceFailureFlow())
    {
        return EXIT_FAILURE;
    }

    std::cout << "플레이어 저장 흐름 테스트 전체 통과\n";

    return EXIT_SUCCESS;
}