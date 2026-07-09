#include "ProcessMonitor.h"
#include <sys/stat.h>
#include <stdio.h>
#include "K_slog.h"

ProcessMonitor::ProcessMonitor()
{}

void ProcessMonitor::AddWatch(int pid, Callback onCrash)
{
    m_items.push_back({pid, onCrash});
}

void ProcessMonitor::Loop()
{
    while(true)
    {
        for (auto& item: m_items)
        {
            struct stat st;
            char procPath[64];
            snprintf(procPath, sizeof(procPath), "/proc/%d", item.pid);
            //K_LOG_DEBUG("check process[%s]", procPath);
            if (stat(procPath, &st) != 0)
            {
                K_LOG_TRACE("Crashed[%s]\n", procPath);
                item.pid = item.cb(); //crash callback
            }
        }
        sleep(LOOP_GAP);
    }
}