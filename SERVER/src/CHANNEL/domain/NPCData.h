#pragma once

#include <optional>
#include <string>
#include <vector>

struct NPCShopComponent
{
    int shopId = 0;
};

//JSON에서 읽어 보관하는 원본 데이터
struct NPCDefinition
{
    int npcId = 0;
    std::string name;
    std::string role;
    std::vector<std::string> dialogue;

    // 상점 기능이 없는 NPC는 비어 있다.
    std::optional<NPCShopComponent> shop;
};

//응답 데이터
struct NPCInteractionResult
{
    int mapId = 0;
    int spawnId = 0;
    int npcId = 0;

    std::string name;
    std::string role;
    std::vector<std::string> dialogue;

    // 0이면 상점 기능이 없다.
    int shopId = 0;
};