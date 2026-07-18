#include "Projectile.h"
#include "K_slog.h"

Projectile::Projectile(const Vec2& startPos,
                const Vec2& dir,
                float speed,
                float range,
                int typeId,
                int ownerId) 
            : m_pos(startPos),
              m_dir(dir),
              m_speed(speed),
              m_range(range),
              m_typeId(typeId),
              m_ownerMonsterId(ownerId)
{
    m_collider.type = ColliderType::Circle2D;
    m_collider.circle.offset = {0.f, 0.f}; // 투사체
    m_collider.circle.radius = 5.f; // 예시 반지름, 필요에 따라 조정
    m_isSendClient = false;
}

void Projectile::Update(float dt)
{
    Vec2 delta;
    delta.xPos = m_dir.xPos * m_speed * dt;
    delta.yPos = m_dir.yPos * m_speed * dt;
    m_pos.xPos += delta.xPos;
    m_pos.yPos += delta.yPos;


    K_LOG_TRACE( "Projectile Update dt[%.2f] pos[%.2f, %.2f] delta[%.2f, %.2f]", dt, m_pos.xPos, m_pos.yPos, delta.xPos, delta.yPos);
    m_travelled += Length(delta);
}

bool Projectile::IsExpired() const
{
    K_LOG_TRACE( "Projectile IsExpired travelled[%.2f] range[%.2f]", m_travelled, m_range);
    return m_travelled >= m_range;
}

Vec2 Projectile::GetPos() const
{
    return m_pos;
}
