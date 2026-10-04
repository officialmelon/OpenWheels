#pragma once
// ONLINE (PC addition): small helpers shared by the misc browser-special ports (Chain, Token,
// Glass, Meteor, Building, Rail, Cannon, Paddle, grouped Van/Bottle). Header-only.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Box2D/Box2D.h"
#include "cocos2d.h"

#include "LevelItem.h"
#include "online/FlashRuntime.h"

namespace online {
namespace misc {

// Flash runs its item logic once per 30 Hz frame; the mobile level steps at 60 Hz (or slower
// in slow motion). Accumulates LevelItem::getTimeStepOverFlashTimeStep() and reports how many
// Flash frames ended during this step.
struct FlashClock {
    float acc = 0.0f;
    int advance(float ratio)
    {
        acc += ratio;
        int frames = 0;
        while (acc >= 1.0f - 1e-4f) {
            acc -= 1.0f;
            frames++;
        }
        return frames;
    }
};

// Largest normal impulse of a solved contact (Flash ContactEvent.RESULT impulse).
inline float maxNormalImpulse(b2Contact* contact, const b2ContactImpulse* impulse)
{
    float m = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount > 1) m = std::max(m, impulse->normalImpulses[1]);
    return m;
}

// First world contact point (falls back to the midpoint of the two bodies).
inline b2Vec2 contactPoint(b2Contact* contact)
{
    b2WorldManifold wm;
    contact->GetWorldManifold(&wm);
    if (contact->GetManifold()->pointCount > 0) return wm.points[0];
    return 0.5f * (contact->GetFixtureA()->GetBody()->GetWorldCenter() +
                   contact->GetFixtureB()->GetBody()->GetWorldCenter());
}

// Contact normal pointing away from `fixture` (Flash b2ContactPoint.normal, shape1 = ours).
inline b2Vec2 normalFrom(b2Contact* contact, b2Fixture* fixture)
{
    b2WorldManifold wm;
    contact->GetWorldManifold(&wm);
    return contact->GetFixtureA() == fixture ? wm.normal : -wm.normal;
}

// |relative normal velocity| at the contact (Flash |b2Dot(point.velocity, point.normal)|).
inline float normalSpeed(b2Contact* contact)
{
    b2WorldManifold wm;
    contact->GetWorldManifold(&wm);
    const b2Vec2 p = contact->GetManifold()->pointCount > 0
                         ? wm.points[0]
                         : contact->GetFixtureA()->GetBody()->GetWorldCenter();
    b2Vec2 v = contact->GetFixtureB()->GetBody()->GetLinearVelocityFromWorldPoint(p) -
               contact->GetFixtureA()->GetBody()->GetLinearVelocityFromWorldPoint(p);
    return std::fabs(b2Dot(v, wm.normal));
}

// Flash "apply impulse" trigger action: (impulseX, impulseY[, spin]) per unit mass, Flash axes.
// Missing properties are NaN in Flash; skip instead of poisoning the body.
inline void applyFlashImpulse(b2Body* body, const std::vector<float>& p, bool withSpin, float mass)
{
    if (!body || p.size() < 2 || !std::isfinite(p[0]) || !std::isfinite(p[1])) return;
    body->ApplyLinearImpulse(b2Vec2(p[0] * mass, -p[1] * mass), body->GetWorldCenter(), true);
    if (withSpin && p.size() > 2 && std::isfinite(p[2])) {
        body->SetAngularVelocity(body->GetAngularVelocity() - p[2]);
    }
}

inline float rand01() { return cocos2d::rand_0_1(); }

// Display points per metre in the gameplay session.
inline float ptm() { return pointsPerFlashPx() * kFlashPtm; }

// Node position (points) for a world point (metres).
inline cocos2d::Vec2 worldPoints(const b2Vec2& m) { return cocos2d::Vec2(m.x * ptm(), m.y * ptm()); }

// Flash px offset inside a node rotated like its Flash parent (y down) -> node-local points.
inline cocos2d::Vec2 localPx(float xPx, float yPx)
{
    const float s = pointsPerFlashPx();
    return cocos2d::Vec2(xPx * s, -yPx * s);
}

// Places a node at a Flash position (px) with a Flash rotation (clockwise degrees).
inline void placeFlash(cocos2d::Node* node, float xPx, float yPx, float rotationDeg)
{
    node->setPosition(worldPoints(flashToWorld(xPx, yPx)));
    node->setRotation(rotationDeg);
}

// Paints a node from a body (position + rotation), like LevelB2D::paint.
inline void paintFromBody(cocos2d::Node* node, b2Body* body, const b2Vec2& at)
{
    node->setPosition(worldPoints(at));
    node->setRotation(body->GetAngle() * -57.29578f);
}

// The rendered Flash symbol, or a plain rectangle (Flash px, centred on (cx, cy) in Flash local
// coordinates) when the art was not extracted.
inline cocos2d::Node* artOr(const std::string& name, float w, float h, const cocos2d::Color4F& color,
                            float cx = 0.0f, float cy = 0.0f)
{
    if (cocos2d::Sprite* s = createFlashSprite(name)) return s;
    auto* d = cocos2d::DrawNode::create();
    const cocos2d::Vec2 a = localPx(cx - w * 0.5f, cy + h * 0.5f);
    const cocos2d::Vec2 b = localPx(cx + w * 0.5f, cy - h * 0.5f);
    d->drawSolidRect(a, b, color);
    return d;
}

// Swaps the texture of a sprite created by createFlashSprite to another rendered frame of the
// same symbol (same canvas, so the anchor stays valid).
inline void setFlashFrame(cocos2d::Sprite* sprite, const std::string& name)
{
    if (!sprite || !hasFlashArt(name)) return;
    auto* tex = cocos2d::Director::getInstance()->getTextureCache()->addImage("generated/flash/" + name + ".png");
    if (!tex || tex == sprite->getTexture()) return;
    sprite->setTexture(tex);
    sprite->setTextureRect(cocos2d::Rect(cocos2d::Vec2::ZERO, tex->getContentSize()));
}

// Flash "glass" rect burst (ParticleController.createRectBurst): shards over the body.
cocos2d::Node* glassBurst(b2Body* body, int count, float rangePx);

}  // namespace misc
}  // namespace online
