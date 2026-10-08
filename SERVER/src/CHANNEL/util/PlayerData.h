#pragma once
#include <string>
#include <cstdint>

class Monster;

enum class RootJob : int
{
    Knight = 20000,
    Mage   = 21000,
    Archer = 22000,
    Thief  = 23000,
    Common = 29000,
    Unknown = 0
};

namespace PlayerData
{
    inline RootJob ToRootJob(int code)
    {
        switch(code)
        {
            case 20000: return RootJob::Knight;
            case 21000: return RootJob::Mage;
            case 22000: return RootJob::Archer;
            case 23000: return RootJob::Thief;
            case 29000: return RootJob::Common;
            default:    return RootJob::Unknown;
        }
    };

}


struct PlayerInitData{
    int char_id;
    std::string account_id;
    std::string name;
    int level;
    int job;
    int root_job;
    int map_id;
    float xPos;
    float yPos;
    std::int64_t gold = 0;
};


// 임시로 여기에 구현 /*2026.02.07 LJH */
// 플레이어가 스킬을 사용했을 때 Hit 당한 몬스터들을 관리하기 위한 구조체
struct Cand {
    Monster* m;
    float front;
};
