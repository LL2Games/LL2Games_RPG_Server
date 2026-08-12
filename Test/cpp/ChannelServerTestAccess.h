#pragma once

#include "ChannelServer.h"
#include "ChannelSession.h"

#include <mutex>

class ChannelServerTestAccess
{
public:
    static void AddSession(ChannelServer& server,ChannelSession* session)
    {
        if (session == nullptr)
            return;

        std::lock_guard<std::mutex> lock(server.m_sessionMutex);
        server.m_sessions[session->GetFd()] = session;
    }

    static void EraseSession(ChannelServer& server, int fd)
    {
        std::lock_guard<std::mutex> lock(server.m_sessionMutex);
        server.m_sessions.erase(fd);
    }

    static void ProcessAuthResults(ChannelServer& server)
    {
        server.ProcessAuthResults();
    }

    static bool TryBeginRun(ChannelServer& server)
    {
        return server.TryBeginRun();
    }
    
    static bool IsRunning(const ChannelServer& server)
    {
        return server.m_running.load(std::memory_order_acquire);
    }
    
    static bool IsStopRequested(const ChannelServer& server)
    {
        return server.m_stopRequested.load(std::memory_order_acquire);
    }
    
    static void ResetLifecycleState(ChannelServer& server)
    {
        server.m_running.store(false, std::memory_order_release);
        server.m_stopRequested.store(false, std::memory_order_release);
    }
    
    static bool WaitForStop(ChannelServer& server, const std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(server.m_stateUpdateWaitMutex);
    
        return server.m_stateUpdateCv.wait_for(lock, timeout, [&server]
        {
            return server.m_stopRequested.load(std::memory_order_acquire);
        });
    }
};