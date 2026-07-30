#pragma once
#include "common.h"
#include "Math.h"
#include "CommonEnum.h"

struct PortalData
{
    std::string portalId;

    int sourceMapId = 0;
    int destinationMapId = 0;

    Vec2 position;
    Vec2 spawnPosition;

    float interactionRange = 100.0f;

    bool IsInInteractionRange(const Vec2& playerPosition) const
    {
        const float dx = playerPosition.xPos - position.xPos;
        const float dy = playerPosition.yPos - position.yPos;
        const float distanceSq = dx * dx + dy* dy;    
        const float rangeSq = interactionRange * interactionRange;
        return distanceSq <= rangeSq;
    }
};

struct PortalMoveResult
{
    bool success = false;
    std::string error;

    int destinationMapId = 0;
    Vec2 spawnPosition;
};