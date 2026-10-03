#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <map>
#include <vector>

class LevelDataElement;

// Level item type 6 (LevelB2D::addSpecial -> Spikes::create). A row of numSpikes spikes that
// impales bodies landing on its sensor strip and paints blood on the spikes it hit.
//
// XML attributes: p0 x, p1 y, p2 angle (deg), p3 fixed (on the level body), p4 numSpikes,
// p5 sleeping.
//
// arm64 sizeof 0x160. No user-provided constructor: create() value-initialises (zero-fill), then
// the containers are constructed and _stabbableMaterials = 3. The first member sits in LevelItem's
// tail padding (0x94). iOS has Spikes : StabbingItem; the Android port flattened StabbingItem's
// members (stabbingBody, sensor, bodiesToAdd, bodiesToRemove, bjMap, angle, stabbableMaterials)
// after Spikes' own (fixed, numSpikes, spikeWidth, spikeHeight, halfWidth, spikeYOffset,
// bloodOffset, mc, body, bloodYMap, spikeBloodCount, soundCounter).
class Spikes : public LevelItem
{
public:
    ~Spikes() override;  // @00630d34 (D2), @00630dac (D0)

    static Spikes* create(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset);  // @005d09ec
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;     // @0062ec00  vptr+0x18

    void createBody(b2Vec2 position, float halfWidth, float angle, bool sleeping);  // @0062f148
    void setupMC();                                                  // @0062f494
    void dealloc();                                                  // @0062f604  (empty)
    void positionBlood(cocos2d::Sprite* blood, unsigned int index);  // @0062f704  (no callers)
    void removeExistingBloodAtIndex(unsigned int index);             // @0062f7e8  (no callers)
    void paintBloodAtPos(b2Vec2 worldPosition);                      // @0062f838
    void soundStopped();                                             // @0062ffb8
    float opacity();                                                 // @0062fff0  1.0 if fully opaque, else 0.0
    void createPrisJoint(b2Body* body);                              // @006301f8
    void removeJoint(b2Body* body);                                  // @006305e4
    void setStabbingBody(b2Body* body);                              // @006307b4  (no callers)

    // LevelItem overrides
    void actions() override;                                         // @00630174  vptr+0x30
    b2Body* getJointBody(b2Vec2 point) override;                     // @0062ffc8  vptr+0x50
    void stopInteractivity() override;                               // @0062f608  vptr+0x58
    void removeSprites() override;                                   // @0062f674  vptr+0x60
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @0063090c  vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @00630aec  vptr+0x90
    void jointWillBeDestroyed(b2Joint* joint) override;              // @00630024  vptr+0xa8
    void fixtureWillBeDestroyed(b2Fixture* fixture) override;        // @006307bc  vptr+0xb0
    void paintWithOffsetPoints(cocos2d::Vec2 offset, float angleDegrees) override;  // @0062f688  vptr+0xc8
    void setOpacity(float opacity) override;                         // @0062ffd0  vptr+0xd0

protected:
    bool _fixed;                              // +0x94  p3
    int _numSpikes;                           // +0x98  p4 (int: converted with scvtf)
    float _spikeWidth;                        // +0x9c  0.24 * ptm (px)
    float _spikeHeight;                       // +0xa0  1.12 * ptm (px)
    float _halfWidth;                         // +0xa4  numSpikes * spikeWidth / 2 (px)
    float _spikeYOffset;                      // +0xa8  0.16 * ptm (px)
    b2Vec2 _bloodOffset;                      // +0xac  init (0,0); unused
    cocos2d::Node* _mc;                       // +0xb8  container of the spike sprites (tag = index for blood)
    b2Body* _body;                            // +0xc0  own body, the group body, or the level body (fixed)
    std::map<unsigned int, float> _bloodYMap; // +0xc8  unused
    std::vector<unsigned int> _spikeBloodCount;  // +0xe0  per spike: blood already painted
    unsigned int _soundCounter;               // +0xf8  "ImpaleSpikes" sounds playing (max 2)
    b2Body* _stabbingBody;                    // +0x100 body of _sensor (= _body)
    b2Fixture* _sensor;                       // +0x108 top sensor strip (begin/end contact listener)
    std::vector<b2Body*> _bodiesToAdd;        // +0x110
    std::vector<b2Body*> _bodiesToRemove;     // +0x128
    std::map<b2Body*, b2PrismaticJoint*> _bjMap;  // +0x140
    float _angle;                             // +0x158 spike angle relative to _stabbingBody (radians)
    unsigned short _stabbableMaterials = 3;   // +0x15c material mask
};
