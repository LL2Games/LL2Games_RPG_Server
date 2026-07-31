#include "ProjectileManager.h"
#include "IProjectile.h"
#include <algorithm>
#include "K_slog.h"

void ProjectileManager::Add(std::unique_ptr<IProjectile> p)
{
    if (!p)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    p->SetInstanceId(m_nextInstanceId++);
    m_projectiles.push_back(std::move(p));
}

void ProjectileManager::Update(float dt)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& p : m_projectiles)
    {
        p->Update(dt);

        //debug
        if (p->IsExpired())
        {
            K_LOG_TRACE("Projectile Expired.. TypeId (%d), InsId (%llu)",
                p->GetTypeId(),
                static_cast<unsigned long long>(p->GetInstanceId()));
        }
    }

    //만료된 투사체를 remove_if로 뒤로 보내고 새로운 End 얻기
    auto newEnd = std::remove_if(m_projectiles.begin(), m_projectiles.end(),
        [](const auto& p){ return p->IsExpired();});

    //새로운 End부터 끝까지의 요소들을 삭제
    m_projectiles.erase(newEnd, m_projectiles.end());
}

std::vector<ProjectileSnapshotInfo> ProjectileManager::CreateSnapshot()
{
    std::vector<ProjectileSnapshotInfo> infos;
	std::lock_guard<std::mutex> lock(m_mutex);
    infos.reserve(m_projectiles.size());

    for (const auto & p : m_projectiles)
    {
        if (!p || p->IsExpired() || p->GetIsSendClient())
        {
            continue;
        }
        p->SetIsSendClient();

        ProjectileSnapshotInfo info{};

        info.projectileTypeId = p->GetTypeId();
        info.ownerMonsterId = p->GetOwnerId();
        info.instanceId = p->GetInstanceId();
        info.dirX = static_cast<int>(p->GetDir().xPos);
        info.dirY = static_cast<int>(p->GetDir().yPos);
        info.range = p->GetRange();
        info.speed = p->GetSpeed();
        info.xPos = p->GetPos().xPos;
        info.yPos = p->GetPos().yPos;

        infos.push_back(info);
    }

    return infos;
}
