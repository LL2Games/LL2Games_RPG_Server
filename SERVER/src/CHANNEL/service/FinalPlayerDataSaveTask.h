#pragma once

#include "Task.h"
#include "PlayerSaveData.h"

class ChannelServer;

class FinalPlayerDataSaveTask  : public Task
{
public:
    FinalPlayerDataSaveTask (ChannelServer* server,PlayerSaveData saveData);
    void Execute() override;

private:
    ChannelServer* m_server = nullptr;
    PlayerSaveData m_saveData;
};