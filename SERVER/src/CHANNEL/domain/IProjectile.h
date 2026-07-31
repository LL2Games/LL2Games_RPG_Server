#pragma once
#include "CommonEnum.h"
#include <cstdint>

class IProjectile
{
public:
    virtual ~IProjectile() = default;
    
    virtual void Update(float dt) = 0;
    virtual bool IsExpired() const = 0;
    virtual Vec2 GetPos() const = 0;
    virtual Vec2 GetDir() const = 0;
    virtual int GetTypeId() const = 0;
    virtual std::uint64_t GetInstanceId() const = 0;
    virtual void SetInstanceId(std::uint64_t instanceId) = 0;
    virtual int GetOwnerId() const = 0;
    virtual Collider2D GetCollider() const = 0; 
    virtual int GetDamage() const = 0; 
    virtual bool GetIsSendClient() const = 0; 
    virtual void SetIsSendClient() = 0; 
    virtual float GetRange() const = 0;
    virtual float GetSpeed() const = 0;
};
