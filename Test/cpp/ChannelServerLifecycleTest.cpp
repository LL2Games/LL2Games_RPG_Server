#include "ChannelServerTestAccess.h"
#include "MapManager.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <thread>

namespace
{
    constexpr int kRaceIterationCount = 5000;
    constexpr int kMapManagerIterationCount = 50;
    constexpr auto kWaitTimeout = std::chrono::seconds(2);
    constexpr auto kImmediateStopLimit = std::chrono::milliseconds(1000);

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

    bool TestStopRequestedBeforeRun()
    {
        ChannelServer server(1, 1, 100);

        server.RequestStop();

        const bool started = ChannelServerTestAccess::TryBeginRun(server);

        if (!Check(!started, "실행 전에 종료 요청이 있으면 서버 실행을 시작하지 않음"))
            return false;

        if (!Check(ChannelServerTestAccess::IsStopRequested(server), "실행 전 종료 요청 상태가 유지됨"))
            return false;

        if (!Check(!ChannelServerTestAccess::IsRunning(server), "실행 전 종료 요청 후 running=false 유지"))
            return false;

        return true;
    }

    bool TestRepeatedRequestStop()
    {
        ChannelServer server(1, 1, 100);

        server.RequestStop();
        server.RequestStop();
        server.RequestStop();

        if (!Check(ChannelServerTestAccess::IsStopRequested(server), "RequestStop 중복 호출 후 stopRequested=true"))
            return false;

        if (!Check(!ChannelServerTestAccess::IsRunning(server), "RequestStop 중복 호출 후 running=false"))
            return false;

        return true;
    }

    bool TestStateUpdateWaitInterrupted()
    {
        ChannelServer server(1, 1, 100);

        ChannelServerTestAccess::ResetLifecycleState(server);

        std::promise<void> waiterStartedPromise;
        std::future<void> waiterStartedFuture = waiterStartedPromise.get_future();
        std::atomic<bool> stopDetected{false};

        std::thread waiter([&server, &waiterStartedPromise, &stopDetected]
        {
            waiterStartedPromise.set_value();

            const bool detected = ChannelServerTestAccess::WaitForStop(server, kWaitTimeout);
            stopDetected.store(detected, std::memory_order_release);
        });

        waiterStartedFuture.wait();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        const auto stopStartedAt = std::chrono::steady_clock::now();

        server.RequestStop();
        waiter.join();

        const auto elapsed = std::chrono::steady_clock::now() - stopStartedAt;

        if (!Check(stopDetected.load(std::memory_order_acquire), "상태 갱신 대기 스레드가 종료 요청을 감지함"))
            return false;

        if (!Check(elapsed < kImmediateStopLimit, "condition_variable 대기 스레드를 1초 이내 깨움"))
            return false;

        return true;
    }

    bool TestStartStopRace()
    {
        ChannelServer server(1, 1, 100);

        for (int iteration = 0; iteration < kRaceIterationCount; ++iteration)
        {
            ChannelServerTestAccess::ResetLifecycleState(server);

            std::atomic<bool> startRace{false};

            std::thread runThread([&server, &startRace]
            {
                while (!startRace.load(std::memory_order_acquire))
                    std::this_thread::yield();

                ChannelServerTestAccess::TryBeginRun(server);
            });

            std::thread stopThread([&server, &startRace]
            {
                while (!startRace.load(std::memory_order_acquire))
                    std::this_thread::yield();

                server.RequestStop();
            });

            startRace.store(true, std::memory_order_release);

            runThread.join();
            stopThread.join();

            const bool stopRequested = ChannelServerTestAccess::IsStopRequested(server);
            const bool running = ChannelServerTestAccess::IsRunning(server);

            if (!stopRequested || running)
            {
                std::cerr << "[FAIL] 실행/종료 경쟁 상태 오류. iteration[" << iteration
                          << "] stopRequested[" << stopRequested
                          << "] running[" << running << "]\n";

                return false;
            }
        }

        return Check(true, "실행 시작과 종료 요청 경쟁 5000회 통과");
    }

    bool TestMapManagerImmediateStop()
    {
        ChannelServer server(1, 1, 100);
        MapManager manager(&server);

        for (int iteration = 0; iteration < kMapManagerIterationCount; ++iteration)
        {
            manager.Start();

            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            const auto stopStartedAt = std::chrono::steady_clock::now();

            manager.Stop();

            const auto elapsed = std::chrono::steady_clock::now() - stopStartedAt;
            const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

            if (elapsed >= kImmediateStopLimit)
            {
                std::cerr << "[FAIL] MapManager 종료 지연. iteration[" << iteration
                          << "] elapsedMs[" << elapsedMs << "]\n";

                return false;
            }

            // 중복 Stop 호출도 안전해야 한다.
            manager.Stop();
        }

        return Check(true, "MapManager Start/Stop 50회 및 중복 Stop 통과");
    }
}

int main()
{
    if (!TestStopRequestedBeforeRun())
        return EXIT_FAILURE;

    if (!TestRepeatedRequestStop())
        return EXIT_FAILURE;

    if (!TestStateUpdateWaitInterrupted())
        return EXIT_FAILURE;

    if (!TestStartStopRace())
        return EXIT_FAILURE;

    if (!TestMapManagerImmediateStop())
        return EXIT_FAILURE;

    std::cout << "ChannelServer 종료 생명주기 테스트 전체 통과\n";

    return EXIT_SUCCESS;
}