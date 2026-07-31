#pragma once

#include "Player.h"

#include <memory>
#include <string>

struct ChannelAuthResult
{
    int fd = -1;
    uint64_t sessionId = 0;
    uint64_t generation = 0;

    int characterId = 0;
    bool success = false;

    std::string error;
    std::unique_ptr<Player> player;
};