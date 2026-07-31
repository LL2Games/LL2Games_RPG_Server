#pragma once
#include "IProjectile.h"
#include "Math.h"
#include <cstdint>

class Projectile : public IProjectile
{
public:
    Projectile(const Vec2& startPos,
                const Vec2& dir,
                float speed,
                float range,
                int typeId,
                int ownerId);

    void Update(float dt) override;
    bool IsExpired() const override;
    Vec2 GetPos() const override;
    Vec2 GetDir() const override { return m_dir; }
    Collider2D GetCollider() const override { return m_collider; }
    virtual int GetDamage() const override { return m_damage; }

    //int GetId() const override { return m_projectileId; }
    int GetTypeId() const override { return m_typeId;}
    std::uint64_t GetInstanceId() const override { return m_instanceId;}
    int GetOwnerId() const override { return m_ownerMonsterId;}
    void SetInstanceId(std::uint64_t instanceId) override { m_instanceId = instanceId; }

    bool GetIsSendClient() const override { return m_isSendClient; }
    void SetIsSendClient() override {m_isSendClient = true; }
    float GetRange() const override { return m_range; }
    float GetSpeed() const override { return m_speed; }
protected:
    bool m_isSendClient = false;

    Vec2 m_pos;
    Vec2 m_dir;
    float m_speed;
    float m_range;
    Collider2D m_collider;
    static constexpr int m_damage = 10; // 예시 데미지, 필요에 따라 조정
    float m_travelled = 0.f;

    int m_typeId;
    int m_ownerMonsterId;
    std::uint64_t m_instanceId = 0;
};
