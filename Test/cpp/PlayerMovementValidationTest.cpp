#include "Player.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <thread>

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
            "movement-test",
            "movement-player",
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

    bool ClearSaveState(Player& player)
    {
        if (!player.IsSaveNeeded())
        {
            return true;
        }

        const PlayerSaveData saveData = player.MakeSaveData();
        return player.TryMarkSaved(saveData.saveVersion);
    }

    bool IsSamePosition(const Vec2& first,const Vec2& second)
    {
        constexpr float kPositionError = 0.001F;
        return std::fabs(first.xPos - second.xPos) <= kPositionError && std::fabs(first.yPos - second.yPos) <= kPositionError;
    }

    bool TestUninitializedMovementRejected()
    {
        Player player;
        InitializePlayer(player);

        std::string errMsg;

        const bool accepted = player.TryApplyMove(Vec2{1.0F, 0.0F}, errMsg);

        if (!Check(!accepted, "검증 초기화 전 이동 거부"))
        {
            return false;
        }

        if (!Check(!errMsg.empty(),"검증 초기화 전 이동의 실패 사유 반환"))
        {
            return false;
        }

        return true;
    }

    bool TestNormalMovementAccepted()
    {
        Player player;
        InitializePlayer(player);

        if (!ClearSaveState(player))
        {
            return Check(false, "정상 이동 테스트의 저장 상태 초기화");
        }

        player.ResetMoveValidation();

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::string errMsg;

        const bool accepted = player.TryApplyMove(Vec2{5.0F, 0.0F}, errMsg);

        if (!Check(accepted,"허용 거리 이내의 정상 이동 승인"))
        {
            std::cerr << "[INFO] 이동 거부 사유: " << errMsg << '\n';
            return false;
        }

        if (!Check(errMsg.empty(), "정상 이동 시 오류 미발생"))
        {
            return false;
        }

        if (!Check(IsSamePosition(player.GetPos(),Vec2{5.0F, 0.0F}), "승인된 좌표를 서버 상태에 반영"))
        {
            return false;
        }

        if (!Check(player.IsSaveNeeded(),"정상 이동 후 DB 저장 필요 상태 표시"))
        {
            return false;
        }

        return true;
    }

    bool TestExcessiveMovementRejected()
    {
        Player player;
        InitializePlayer(player);

        if (!ClearSaveState(player))
        {
            return Check(false, "비정상 이동 테스트의 저장 상태 초기화");
        }

        player.ResetMoveValidation();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        const Vec2 originalPosition = player.GetPos();

        std::string errMsg;

        const bool accepted = player.TryApplyMove(Vec2{1000.0F, 1000.0F},errMsg);

        if (!Check(!accepted,"허용 거리를 초과한 이동 거부"))
        {
            return false;
        }

        if (!Check(!errMsg.empty(),"거리 초과 이동의 실패 사유 반환"))
        {
            return false;
        }

        if (!Check(IsSamePosition(player.GetPos(),originalPosition),"거부된 좌표를 서버 상태에 반영하지 않음"))
        {
            return false;
        }

        if (!Check(!player.IsSaveNeeded(),"거부된 이동은 DB 저장 상태를 변경하지 않음"))
        {
            return false;
        }

        return true;
    }

    bool TestInvalidPositionRejected()
    {
        Player player;
        InitializePlayer(player);

        if (!ClearSaveState(player))
        {
            return Check(false, "비정상 좌표 테스트의 저장 상태 초기화");
        }

        player.ResetMoveValidation();

        const Vec2 originalPosition = player.GetPos();

        std::string errMsg;

        const bool nanAccepted = player.TryApplyMove(Vec2{std::numeric_limits<float>::quiet_NaN(),0.0F},errMsg);

        if (!Check(!nanAccepted,"NaN 좌표 거부"))
        {
            return false;
        }

        errMsg.clear();

        const bool infinityAccepted = player.TryApplyMove(Vec2{0.0F,std::numeric_limits<float>::infinity()},errMsg);

        if (!Check(!infinityAccepted,"무한대 좌표 거부"))
        {
            return false;
        }

        if (!Check(IsSamePosition(player.GetPos(),originalPosition),"비정상 좌표 거부 후 기존 위치 보존"))
        {
            return false;
        }

        if (!Check(!player.IsSaveNeeded(),"비정상 좌표는 DB 저장 상태를 변경하지 않음"))
        {
            return false;
        }

        return true;
    }

    bool TestMapEntryReset()
    {
        Player player;
        InitializePlayer(player);

        // 포털 이동으로 서버가 목적지 좌표를 설정한 상황
        player.SetPos(Vec2{500.0F, 300.0F});

        if (!ClearSaveState(player))
        {
            return Check(false, "맵 입장 테스트의 저장 상태 초기화");
        }

        // MapInstance::OnEnter()에서 호출되는 초기화
        player.ResetMoveValidation();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        std::string errMsg;

        const bool accepted = player.TryApplyMove(Vec2{502.0F, 300.0F},errMsg);

        if (!Check(accepted,"맵 입장 좌표 기준의 정상 이동 승인"))
        {
            std::cerr << "[INFO] 이동 거부 사유: " << errMsg << '\n';
            return false;
        }

        if (!Check(IsSamePosition(player.GetPos(), Vec2{502.0F, 300.0F}),"맵 입장 후 이동 좌표 반영"))
        {
            return false;
        }

        return true;
    }
}

int main()
{
    if (!TestUninitializedMovementRejected())
    {
        return EXIT_FAILURE;
    }

    if (!TestNormalMovementAccepted())
    {
        return EXIT_FAILURE;
    }

    if (!TestExcessiveMovementRejected())
    {
        return EXIT_FAILURE;
    }

    if (!TestInvalidPositionRejected())
    {
        return EXIT_FAILURE;
    }

    if (!TestMapEntryReset())
    {
        return EXIT_FAILURE;
    }

    std::cout << "플레이어 이동 검증 테스트 전체 통과\n";

    return EXIT_SUCCESS;
}