#pragma once
// Float-exact Box2D joint getters for the Win32 build.
//
// The original game (arm64) calls b2RevoluteJoint::GetJointAngle()/GetJointSpeed() from Box2D
// built for arm64, where every operation is a rounded 32-bit float op:
//     GetJointAngle() = (bB->m_sweep.a - bA->m_sweep.a) - m_referenceAngle
//     GetJointSpeed() =  bB->m_angularVelocity - bA->m_angularVelocity
// cocos2d-x's prebuilt Win32 libbox2d.lib evaluates these return expressions on the x87 stack
// (`flds; fsubs; fsubs; ret`, 53-bit precision) and hands back an unrounded st(0), which MSVC
// callers then feed straight into further x87 arithmetic. GetJointAngle is thus rounded once
// instead of twice and results differ in the last bits (parity: docs/PARITY.md, pink_nightmare
// frame 75, CharacterB2D::resetJointLimits). These helpers compute the same expressions from
// Box2D's inline accessors, force-inlined so MSVC keeps them in SSE float arithmetic.
// On arm64 they compile to exactly what the original's out-of-line Box2D getters compute.

#include <cmath>

#include "Box2D/Box2D.h"

#if defined(_MSC_VER)
#define OW_B2_FORCEINLINE __forceinline
#else
#define OW_B2_FORCEINLINE inline
#endif

namespace owb2 {

// b2RevoluteJoint::GetJointAngle()
OW_B2_FORCEINLINE float32 jointAngle(b2RevoluteJoint* joint)
{
    float32 relative = joint->GetBodyB()->GetAngle() - joint->GetBodyA()->GetAngle();
    return relative - joint->GetReferenceAngle();
}

// b2RevoluteJoint::GetJointSpeed()
OW_B2_FORCEINLINE float32 jointSpeed(b2RevoluteJoint* joint)
{
    return joint->GetBodyB()->GetAngularVelocity() - joint->GetBodyA()->GetAngularVelocity();
}

// b2Mul(const b2Transform&, const b2Vec2&) (inline in b2Math.h, used by b2Body::GetWorldPoint) as
// the original's clang (-ffp-contract=on) compiles it into game code:
//   x = fmuladd(q.c, v.x, -(q.s * v.y)) + p.x,   y = fmuladd(q.s, v.x, q.c * v.y) + p.y
// (asm e.g. RoadBike::leanForwardButtonPressed @0060e034/@0060e040). MSVC never fuses; use this
// where the bisection showed the fused rounding matters.
OW_B2_FORCEINLINE b2Vec2 mulContracted(const b2Transform& T, const b2Vec2& v)
{
    float32 x = std::fma(T.q.c, v.x, -(T.q.s * v.y)) + T.p.x;
    float32 y = std::fma(T.q.s, v.x, T.q.c * v.y) + T.p.y;
    return b2Vec2(x, y);
}

// b2Body::GetWorldPoint(localPoint) with the original's contraction (see mulContracted).
OW_B2_FORCEINLINE b2Vec2 worldPoint(const b2Body* body, const b2Vec2& localPoint)
{
    return mulContracted(body->GetTransform(), localPoint);
}

}  // namespace owb2
