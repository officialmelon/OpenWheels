#pragma once

// TargetRaycast: b2RayCastCallback used by ArrowGun/HarpoonGun to check line of sight to their
// target body. Built on the caller's stack: callers set _closestFraction = 1.0f and
// _targetBody, then read _hitBody (closest blocking body or the target). sizeof 0x20 (arm64).
// Destructor is implicit (vtable slot 0 is b2RayCastCallback's).

#include "Box2D/Box2D.h"

class TargetRaycast : public b2RayCastCallback
{
public:
    virtual float32 ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal,
                                  float32 fraction) override;

    float _closestFraction;  // +0x08
    b2Body* _targetBody;     // +0x10
    b2Body* _hitBody;        // +0x18 (not initialised by the callers)
};
