#pragma once

#include "ChannelServer.h"
#include "ChannelSession.h"

#include <mutex>

class ChannelServerTestAccess
{
public:
    static void AddSession(
        ChannelServer& server,
        ChannelSession* session)
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
};