#pragma once

#include "Task.h"
#include "MapInstance.h"

class MapUpdateTask : public Task
{
public:
    MapUpdateTask(MapInstance* map, float deltaTime) : m_map(map), m_deltaTime(deltaTime) {}
    void Execute() override
    {
        if (m_map)
            m_map->Update(m_deltaTime); //여기서 몬스터 이동처리
    }
private:
    MapInstance* m_map;
    float m_deltaTime = 0.0f;
};