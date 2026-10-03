#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <vector>

class LevelDataElement;
class Sound;

// Level item type 4 (LevelB2D::addSpecial: new(nothrow) Log() + init; the ctor is inlined there).
// A breakable wooden beam: a box body (or, when fixed, a box on the level body). Contacts play
// "LumberHit<1|2>" (ceil(rand/RAND_MAX * 2)) (handleContactAdds, via contactSoundHandler from beginContact); a post-solve
// impulse above _maxImpulse (20) breaks it (singleAction): woodchip burst, the beam is replaced by
// two half-height bodies with "log_bottom.png"/"log_top.png", "LumberBreak".
//
// XML attributes: p0 x, p1 y, p2 width, p3 height, p4 angle (deg), p5 fixed, p6 sleeping.
//
// arm64 sizeof 0xf0. The constructor is user-provided but inline (no symbol): it initialises every
// member below except _center (0xc8, a b2Vec2 whose default ctor does nothing), with
// _maxImpulse = 20.0f. Destructor trivial (vtable slot 0 is LevelItem's D1). Members are the iOS
// ivars in order.
class Log : public LevelItem
{
public:
    // Inline: inlined into LevelB2D::addSpecial, no symbol.
    Log() {}
    ~Log() override;  // @005e75d0 (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @005e6420  vptr+0x18

    // fixed: box on the level body at _center; else a dynamic body (user data _mc).
    void createBody(b2Vec2 position, float angle, float width, float height, bool fixed,
                    bool sleeping);                                  // @005e69ec

    // LevelItem overrides
    void actions() override;                                         // @005e6c34  vptr+0x30  -> handleContactAdds()
    void singleAction() override;                                    // @005e6e98  vptr+0x38  break
    b2Body* getJointBody(b2Vec2 point) override;                     // @005e7584  vptr+0x50  _body
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @005e6c24  vptr+0x88  -> contactSoundHandler(.., nullptr)
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;        // @005e6bb0  vptr+0xa0
    void handleContactAdds() override;                               // @005e6c40  vptr+0xc0
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;  // @005e7420  vptr+0xd8
    std::vector<b2Body*> getBodyList() override;                     // @005e758c  vptr+0xf0

    // Sound finish callback (the handleContactAdds lambda inlines it): _sound = nullptr.
    void soundStopped();                                             // @005e6e90  (no callers)

protected:
    cocos2d::Node* _mc = nullptr;           // +0x98  container (bottom, top, center sprites)
    cocos2d::Sprite* _bottomMC = nullptr;   // +0xa0  "log_bottom.png"
    cocos2d::Sprite* _topMC = nullptr;      // +0xa8  "log_top.png"
    cocos2d::Sprite* _centerMC = nullptr;   // +0xb0  "log_center.png"
    b2Body* _body = nullptr;                // +0xb8  nullptr when fixed / after breaking
    float _shapeWidthMeters = 0.0f;         // +0xc0  p2
    float _shapeHeightMeters = 0.0f;        // +0xc4  p3
    b2Vec2 _center;                         // +0xc8  position when fixed (not initialised otherwise)
    float _rotation = 0.0f;                 // +0xd0  p4 in degrees, then radians (* -0.017453292f)
    b2Fixture* _shape = nullptr;            // +0xd8
    float _maxImpulse = 20.0f;              // +0xe0  breaking impulse
    Sound* _sound = nullptr;                // +0xe8  current "LumberHit" sound
};
