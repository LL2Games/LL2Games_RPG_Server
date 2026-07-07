#pragma once

#include "Player.h"

#include <memory>
#include <string>

struct ChannelAuthResult
{
    int fd = -1;
    int characterId = 0;
    bool success = false;
    std::string error;

    std::unique_ptr<Player> player;
};