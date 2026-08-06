#include "FinalPlayerDataSaveTask.h"

#include "ChannelServer.h"
#include "PlayerDataSaveService.h"

#include <chrono>
#include <exception>
#include <string>
#include <thread>
#include <utility>

namespace
{
    constexpr int kMaxSaveAttempts = 3;
    constexpr int kRetryDelayMilliseconds = 100;
}

FinalPlayerDataSaveTask::FinalPlayerDataSaveTask(ChannelServer* server,PlayerSaveData saveData)
    : m_server(server),
      m_saveData(std::move(saveData))
{
}

void FinalPlayerDataSaveTask::Execute()
{
    if (m_server == nullptr)
    {
        return;
    }

    bool saveSucceeded = false;
    std::string errMsg;

    try
    {
        PlayerDataSaveService* saveService = m_server->GetPlayerDataSaveService();

        if (saveService == nullptr)
        {
            errMsg ="Player data save service is not available";
        }
        else
        {
            for (int attempt = 1; attempt <= kMaxSaveAttempts; ++attempt)
            {
                if (saveService->SavePlayerData(m_saveData,errMsg))
                {
                    saveSucceeded = true;
                    break;
                }

                if (attempt < kMaxSaveAttempts)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(kRetryDelayMilliseconds * attempt));
                }
            }
        }
    }
    catch (const std::exception& exception)
    {
        errMsg = exception.what();
    }
    catch (...)
    {
        errMsg = "Unknown final player save error";
    }

    m_server->CompleteFinalPlayerDataSave(m_saveData.characterId, saveSucceeded,errMsg);
}