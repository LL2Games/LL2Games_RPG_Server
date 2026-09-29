#pragma once
#include "Math.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Movement {
    constexpr float StepSeconds = 1.0f / 60.0f;
    constexpr float Gravity = 1800.0f;
    constexpr float MaxFall = 1000.0f;
    constexpr float JumpSpeed = 650.0f;
    constexpr float ClimbSpeed = 180.0f;
    constexpr float Epsilon = 0.05f;

    enum class Mode : int { Grounded = 0, Rising = 1, Falling = 2, Climbing = 3 };
    enum class ClimbKind : int { Rope = 0, Ladder = 1 };

    struct Platform { 
        int id = 0;
        float left = 0, right = 0, y = 0;
    };

    struct Climbable {
        int id = 0;
        ClimbKind kind = ClimbKind::Ladder;
        float x  = 0, top = 0, bottom = 0, grabRange = 16;
        int topPlatformId = 0; // 최상단에서 올라설 발판 ID
    };

    struct Map {
        std::vector<Platform> platforms;
        std::vector<Climbable> climbables;
        float minX = 0, maxX = 1600, killY = 1200;
        Vec2 safeFeet{100, 700}; // 객체 원점이 아니라 발 좌표
    };

    struct Input {
        int horizontal = 0;     // -1, 0, 1
        int vertical = 0;       // -1=위, 0, 1=아래
        bool jump = false;      // 누른 순간 한 번만 true
    };

    struct Body {
        float vx = 0, vy = 0;
        float footOffset = 0;
        float grabCooldown = 0;
        Mode mode = Mode::Falling;
        int platformId = 0;
        int climbableId = 0;
        int facing = 1;
    };

    struct Result { bool landed = false; bool fellOut = false; };

    inline const Platform* FindPlatform(const Map& map, int id) {
        for (const auto & p : map.platforms) if (p.id == id) return &p;
        return nullptr;
    }

    inline const Climbable* FindClimbable(const Map& map, int id) {
        for (const auto& c : map.climbables) if (c.id == id) return &c;
        return nullptr;
    }

    inline bool Covers(const Platform& p, float x) { 
        return x >= p.left && x <= p.right;
    }

    inline const Platform *Support(const Map& map, float x, float feetY) {
        for (const auto& p : map.platforms)
            if (Covers(p, x) && std::fabs(feetY - p.y) <= Epsilon) return &p;
        return nullptr;
    }

    inline void Reset(Body& body) {
        const float offset = body.footOffset;
        const int facing = body.facing;
        body = Body{};
        body.footOffset = offset;
        body.facing = facing;
    }

    inline void PlaceOnFeet(Vec2& pos, Body& body, const Map& map, Vec2 feet) {
        Reset(body);
        pos = {feet.xPos, feet.yPos - body.footOffset};
        if (const auto *p = Support(map, feet.xPos, feet.yPos)) {
            body.mode = Mode::Grounded;
            body.platformId = p->id;
        }
    }

    //초기 위치는 원점 좌표다. 공중이면 그 자리에서 낙하를 시작한다.
    inline void ResetAtOrigin(Vec2& pos, Body& body, const Map& map) {
        PlaceOnFeet(pos, body, map, {pos.xPos, pos.yPos + body.footOffset});
    }

    inline Result Step(Vec2& pos, Body& body, const Map& map, Input input, float moveSpeed, float dt) {
        Result result;
        if (!(dt > 0) || !std::isfinite(dt)) return result;
        input.horizontal = std::clamp(input.horizontal, -1, 1);
        input.vertical = std::clamp(input.vertical, -1, 1);
        body.grabCooldown = std::max(0.0f, body.grabCooldown - dt);
        if (input.horizontal != 0) body.facing = input.horizontal;

        if (body.mode == Mode::Grounded) {
            const auto* support = Support(map, pos.xPos, pos.yPos + body.footOffset);
            if (!support) {body.mode = Mode::Falling; body.platformId = 0;}
        }

        // 점프를 누른 프레임에는 새 사다리를 잡지 않는다.
        if (body.mode != Mode::Climbing && input.vertical != 0 && !input.jump && body.grabCooldown <= 0) {
            const float feetY = pos.yPos + body.footOffset;
            for (const auto & c : map.climbables) {
                if (std::fabs(pos.xPos - c.x) <= c.grabRange &&
                    feetY >= c.top - Epsilon && feetY <= c.bottom + Epsilon) {
                        body.mode = Mode::Climbing;
                        body.climbableId = c.id;
                        body.platformId = 0;
                        body.vx = body.vy = 0;
                        pos.xPos = c.x;
                        break;
                }
            }
        }

        if (body.mode == Mode::Climbing) {
            const auto* c = FindClimbable(map, body.climbableId);
            if (!c || input.jump) {
                body.mode = input.jump ? Mode::Rising : Mode::Falling;
                body.vy = input.jump ? -JumpSpeed : 0;
                body.climbableId = 0;
                body.grabCooldown = 0.2f; // 점프로 나가자마자 다시 잡히는것 방지
            } else {
                pos.xPos = c->x;
                body.vx = 0;
                body.vy = input.vertical * ClimbSpeed;
                float feetY = pos.yPos + body.footOffset + body.vy * dt;
                if (input.vertical < 0 && feetY <= c->top) {
                    //로더가 topPlatformId의 위치를 미리 검증한다.
                    const auto* top = FindPlatform(map, c->topPlatformId);
                    if (top) {
                        PlaceOnFeet(pos, body, map, {c->x, top->y});
                        body.grabCooldown = 0.2f;
                        return result;
                    }
                }
                if (input.vertical > 0 && feetY >= c->bottom) {
                    feetY = c->bottom;
                    PlaceOnFeet(pos, body, map, {c->x, feetY});
                    body.grabCooldown = 0.2f;
                    return result;
                }
                pos.yPos = std::clamp(feetY, c->top, c->bottom) - body.footOffset;
                return result;
            }
        }

        if (input.jump && body.mode == Mode::Grounded) {
            body.vy = -JumpSpeed;
            body.mode = Mode::Rising;
            body.platformId = 0;
        }
        body.vx = input.horizontal * std::max(0.0f, moveSpeed);
        const Vec2 before = pos;
        const float nextX = std::clamp(pos.xPos + body.vx * dt, map.minX, map.maxX);

        if (body.mode == Mode::Grounded) {
            const auto* p = FindPlatform(map, body.platformId);
            if (p && Covers(*p, nextX)) {
                pos.xPos = nextX;
                pos.yPos = p->y - body.footOffset;
                body.vy = 0;
                return result;
            }
            body.mode = Mode::Falling;
            body.platformId = 0;
        }

        body.vy = std::min(body.vy + Gravity * dt, MaxFall);
        const float nextY = before.yPos + body.vy * dt;
        const float oldFeet = before.yPos + body.footOffset;
        const float newFeet = nextY + body.footOffset;
        const Platform* hit = nullptr;
        float bestT = 2.0f;

        // 이전 발 높이 -> 다음 발 높이의 선분으로 검사: 빠르게 떨어져도 관통하지 않는다.
        if (body.vy >= 0 && newFeet > oldFeet) {
            for (const auto &p : map.platforms) { 
                if (oldFeet > p.y + Epsilon || newFeet < p.y) continue;
                const float t = std::clamp((p.y - oldFeet) / (newFeet - oldFeet), 0.0f, 1.0f);
                const float hitX = before.xPos + (nextX - before.xPos) * t;
                if (Covers(p, hitX) && t < bestT) { hit = &p, bestT = t; }
            }
        }
        pos = {nextX, nextY};
        if (hit) {
            result.landed = true;
            body.vy = 0;
            // 착지 후 같은 스텝에 가장자리를 지나면 다음 스텝부터 낙하
            pos.yPos = hit->y - body.footOffset;
            body.mode = Covers(*hit, nextX) ? Mode::Grounded : Mode::Falling;
            body.platformId = body.mode == Mode::Grounded ? hit->id : 0;
        } else {
            body.mode = body.vy < 0 ? Mode::Rising : Mode::Falling;
        }
        result.fellOut = pos.yPos + body.footOffset > map.killY;
        return result;
    }
} // namespace Movement