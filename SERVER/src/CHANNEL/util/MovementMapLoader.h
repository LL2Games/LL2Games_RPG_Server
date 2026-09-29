#pragma once
#include "MovementPhysics.h"
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <string>

namespace Movement {
inline Map LoadMap(const nlohmann::json& root) {
    const auto& j = root.at("physics");
    Map map;
    auto number = [](const nlohmann::json& obj, const char* key) {
        const float value = obj.at(key).get<float>();
        if (!std::isfinite(value)) throw std::runtime_error("non-finite physics value");
        return value;
    };
    map.minX = number(j, "minX");
    map.maxX = number(j, "maxX");
    map.killY = number(j, "killY");
    map.safeFeet = {number(j.at("safeFeet"), "x"), number(j.at("safeFeet"), "y")};
    if (map.minX >= map.maxX) throw std::runtime_error("invalid world bounds");
    std::set<int> ids;
    for (const auto& v : j.at("platforms")) {
        Platform p{v.at("id").get<int>(), number(v,"left"), number(v,"right"), number(v,"y")};
        if (p.id <= 0 || !ids.insert(p.id).second || p.left >= p.right ||
            p.left < map.minX || p.right > map.maxX || p.y >= map.killY)
            throw std::runtime_error("invalid platform");
        map.platforms.push_back(p);
    }
    if (!Support(map, map.safeFeet.xPos, map.safeFeet.yPos))
        throw std::runtime_error("safeFeet must be on a platform");
    ids.clear();
    for (const auto& v : j.at("climbables")) {
        Climbable c;
        c.id = v.at("id").get<int>();
        const std::string kind = v.at("kind").get<std::string>();
        if (kind != "rope" && kind != "ladder") throw std::runtime_error("invalid climb kind");
        c.kind = kind == "rope" ? ClimbKind::Rope : ClimbKind::Ladder;
        c.x = number(v,"x"); c.top = number(v,"top"); c.bottom = number(v,"bottom");
        c.grabRange = number(v,"grabRange");
        c.topPlatformId = v.at("topPlatformId").get<int>();
        const auto* p = FindPlatform(map, c.topPlatformId);
        if (c.id <= 0 || !ids.insert(c.id).second || c.top >= c.bottom ||
            c.grabRange <= 0 || c.x < map.minX || c.x > map.maxX || c.bottom >= map.killY ||
            !p || !Covers(*p,c.x) || std::fabs(p->y-c.top) > Epsilon)
            throw std::runtime_error("invalid climbable or top platform");
        map.climbables.push_back(c);
    }
    return map;
}
} // namespace Movement