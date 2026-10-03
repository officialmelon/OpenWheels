#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <vector>

class LevelDataElement;

// Level item type 0 (LevelB2D::addSpecial: new(nothrow) Van() + init). A van: body box plus two
// tire bodies on revolute joints. A hard hit (post-solve impulse > 150) smashes it: the body
// fixture is replaced, the sprite swapped for "van_smashed.png" and the wheels fall off.
//
// XML attributes: p0 x, p1 y, p2 angle (deg), p3 sleeping, p4 interactive (default true; false =
// sprites only, wheels parented to _mc).
//
// arm64 sizeof 0xe0. No user-provided constructor (factory value-initialises = zero-fill; the
// cocos2d::Vec2 member is then re-zeroed by its own ctor) and a trivial destructor (vtable slot 0
// is LevelItem's D1). Members are the iOS ivars in order (body, wheelPosPixels, wheelSize, mc,
// fixture, tire1, tire2, leftJoint, rightJoint).
// Uses LevelItem's _contactAddSounds map (+0x40): "VanHit" for _fixture, "CarTire1" for the tires.
class Van : public LevelItem
{
public:
    ~Van() override;  // @0063d2e4 (D0; D1 is LevelItem's)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @0063c17c  vptr+0x18

    void createBodyAt(b2Vec2 position, float angleDegrees, bool sleeping);  // @0063cab0
    void createJoints();                                    // @0063ccc0  (no callers; init inlines it)

    // LevelItem overrides
    void actions() override;                                // @0063cdac  vptr+0x30  -> handleContactAdds()
    void singleAction() override;                           // @0063cdb8  vptr+0x38  smash
    b2Body* getJointBody(b2Vec2 point) override;            // @0063d168  vptr+0x50
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @0063d158  vptr+0x88  -> contactSoundHandler(.., nullptr)
    void postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                   const b2ContactImpulse* impulse) override;       // @0063d1b4  vptr+0xa0
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;  // @0063cff4  vptr+0xd8
    std::vector<b2Body*> getBodyList() override;            // @0063d170  vptr+0xf0

protected:
    b2Body* _body;                            // +0x98  van body (user data = _mc)
    cocos2d::Vec2 _wheelPosPixels;            // +0xa0  (0.928, 0.768) * ptm
    b2Vec2 _wheelSize;                        // +0xa8  (0.16, 0.36) m, tire box half extents
    cocos2d::Node* _mc;                       // +0xb0  container: 2x "van.png" halves + "van_label.png"
    b2Fixture* _fixture;                      // +0xb8  van body box (post-solve + begin-contact listener)
    b2Fixture* _tire1;                        // +0xc0  left tire (own body)
    b2Fixture* _tire2;                        // +0xc8  right tire (own body)
    b2RevoluteJoint* _leftJoint;              // +0xd0
    b2RevoluteJoint* _rightJoint;             // +0xd8
};
