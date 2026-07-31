#pragma once
#include <memory>
#include <vector>
#include <mutex>
#include "CommonEnum.h"

class IProjectile;

class ProjectileManager
{
public:
    void Add(std::unique_ptr<IProjectile> p);
    void Update(float dt = 1.0f);

    //mutex 동기화등의 처리 필요
    const std::vector<std::unique_ptr<IProjectile>>& GetProjectiles() const { return m_projectiles; }
    std::vector<ProjectileSnapshotInfo> CreateSnapshot();

private:
    std::mutex m_mutex;
    std::vector<std::unique_ptr<IProjectile>> m_projectiles;
    std::uint64_t m_nextInstanceId = 1; //인스턴스 아이디 맵안에서 겹치지 않으면 됌
};