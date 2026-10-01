#include "Player.h"
#include "GameplayGate.h"

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

namespace
{
    bool TestMovementInputBuffer()
    {
        Player player;
        InitializePlayer(player);
        player.ResetMovement();
        const int epoch = player.MovementEpoch();
        if (!Check(player.AcceptMovement(epoch, 1, {1, 0, true}), "새 이동 입력 수락")) return false;
        if (!Check(player.AcceptMovement(epoch, 2, {0, 0, false}), "같은 갱신 전 키 해제 수락")) return false;
        const auto first = player.ConsumeMovement(0.01f);
        if (!Check(first.horizontal == 0 && first.jump, "키 해제 후에도 점프 누름 1회 보존")) return false;
        if (!Check(!player.ConsumeMovement(0.01f).jump, "점프는 한 번만 소비")) return false;
        if (!Check(player.AcceptMovement(epoch, 3, {-1, 1, false}), "좌측/아래 입력 수락")) return false;
        const auto held = player.ConsumeMovement(0.1f);
        if (!Check(held.horizontal == -1 && held.vertical == 1, "누르고 있는 방향 유지")) return false;
        const auto expired = player.ConsumeMovement(0.21f);
        if (!Check(expired.horizontal == 0 && expired.vertical == 0 && !expired.jump,
                   "300ms 초과 입력 중단 시 방향 해제")) return false;
        if (!Check(player.AcceptMovement(epoch, 4, {1, 0, false}), "timeout 뒤 새 입력으로 복구")) return false;
        const auto invalidTime = player.ConsumeMovement(std::numeric_limits<float>::quiet_NaN());
        return Check(invalidTime.horizontal == 0 && !invalidTime.jump, "잘못된 dt는 입력 해제");
    }

    bool TestMovementInputRejection()
    {
        Player player;
        InitializePlayer(player);
        player.ResetMovement();
        const int epoch = player.MovementEpoch();
        if (!Check(!player.AcceptMovement(epoch, 0, {}), "0번 순번 거부")) return false;
        if (!Check(player.AcceptMovement(epoch, 5, {1, 0, false}), "기준 입력 수락")) return false;
        if (!Check(!player.AcceptMovement(epoch, 5, {}) && !player.AcceptMovement(epoch, 4, {}),
                   "중복/역순 입력 거부")) return false;
        if (!Check(!player.AcceptMovement(epoch - 1, 6, {}) && !player.AcceptMovement(epoch + 1, 6, {}),
                   "다른 epoch 입력 거부")) return false;
        if (!Check(!player.AcceptMovement(epoch, 6, {2, 0, false}) &&
                   !player.AcceptMovement(epoch, 6, {0, -2, false}), "범위 밖 축 입력 거부")) return false;
        player.SetState(PlayerState::DEAD);
        if (!Check(!player.AcceptMovement(epoch, 6, {}), "사망 중 입력 거부")) return false;
        player.SetState(PlayerState::STUNNED);
        if (!Check(!player.AcceptMovement(epoch, 6, {}), "기절 중 입력 거부")) return false;
        if (!Check(player.LastInputSequence() == 5, "거부된 입력은 수락 순번을 변경하지 않음")) return false;
        player.SetState(PlayerState::IDLE);
        const auto input = player.ConsumeMovement(0.01f);
        return Check(input.horizontal == 1, "거부된 입력은 저장된 방향을 변경하지 않음");
    }

    bool TestMovementReset()
    {
        Player player;
        InitializePlayer(player);
        if (!Check(player.GetState() == PlayerState::IDLE && player.GetDir() == 1,
                   "생성 시 상태와 방향 초기화")) return false;
        if (!Check(player.MovementBody().footOffset == 10.0f, "현재 충돌체 기준 발 오프셋 10")) return false;
        const auto before = player.GetPos();
        const int epoch = player.MovementEpoch();
        if (!player.AcceptMovement(epoch, 8, {1, -1, true})) return false;
        auto& body = player.MovementBody();
        body.vx = 100; body.vy = -200; body.facing = -1;
        body.mode = Movement::Mode::Climbing; body.climbableId = 3;
        body.platformId = 7; body.grabCooldown = 0.2f;
        player.ResetMovement();
        if (!Check(player.MovementEpoch() == epoch + 1 && player.LastInputSequence() == 0,
                   "초기화 시 epoch 증가 및 순번 초기화")) return false;
        if (!Check(body.vx == 0 && body.vy == 0 && body.climbableId == 0 && body.platformId == 0 &&
                   body.grabCooldown == 0 && body.mode == Movement::Mode::Falling,
                   "속도/발판/탑승 상태 초기화")) return false;
        if (!Check(body.footOffset == 10 && body.facing == -1, "초기화 시 발 오프셋과 물리 방향 보존")) return false;
        const auto cleared = player.ConsumeMovement(0);
        if (!Check(cleared.horizontal == 0 && cleared.vertical == 0 && !cleared.jump,
                   "초기화 시 보관 입력 해제")) return false;
        if (!Check(!player.AcceptMovement(epoch, 9, {}) &&
                   player.AcceptMovement(player.MovementEpoch(), 1, {}),
                   "초기화 이전 입력 거부, 새 epoch의 1번 입력 수락")) return false;
        player.SetFacing(-1);
        player.SetFacing(0);
        if (!Check(player.GetDir() == -1, "방향은 -1/1만 허용")) return false;
        return Check(IsSamePosition(before, player.GetPos()), "입력 보관/초기화만으로 위치를 이동시키지 않음");
    }
}

int main()
{
    Gameplay::Guard guard(Gameplay::gate);
    if (!TestMovementInputBuffer() || !TestMovementInputRejection() || !TestMovementReset())
        return EXIT_FAILURE;

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