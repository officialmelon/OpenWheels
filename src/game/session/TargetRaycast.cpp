#include "TargetRaycast.h"

// Implicit deleting destructor @00581d4c (tail call to operator delete) is emitted here with the
// vtable.

// @00581d50
float32 TargetRaycast::ReportFixture(b2Fixture* fixture, const b2Vec2& point,
                                     const b2Vec2& normal, float32 fraction)
{
    // Sensors never block. A closer fixture is recorded when it belongs to the target body, or
    // when it is heavy (density >= 10) or static and collides with category 0x0008.
    // Always returns 1 (continue the ray over its full length).
    if (!fixture->IsSensor() && fraction < _closestFraction)
    {
        b2Body* body = fixture->GetBody();
        if (body == _targetBody ||
            ((fixture->GetDensity() >= 10.0f || body->GetType() == b2_staticBody) &&
             (fixture->GetFilterData().maskBits & 0x0008) != 0))
        {
            _closestFraction = fraction;
            _hitBody = body;
        }
    }
    return 1.0f;
}
