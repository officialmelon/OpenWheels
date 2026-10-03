#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <vector>

class LevelDataElement;
class Sound;

// Level item type 0xc (LevelB2D::addSpecial: new(nothrow) BoostPanel() + init). A strip of numPanels
// boost panels: a sensor box on the level body; every dynamic body touching it gets
// mass * power pushed along the panel direction each frame (frameAction), with a looping
// "BoostLoop3" body sound. Panel art cycles through "boostpanel_1..4.png" (sprite tag = frame,
// advanced every 4th frame, wrapping 9 -> 1; tags >= 5 keep the last frame shown).
// Trigger: prepareForTrigger stops it (frames greyed out, opacity 0x99); the first
// triggerSingleActivation starts it.
//
// XML attributes: p0 x, p1 y, p2 angle (deg), p3 numPanels (int, default 1), p4 power (int,
// default 20).
//
// arm64 sizeof 0x110. Out-of-line ctor zeroes every member. The first member sits in LevelItem's
// tail padding (0x94). Members are the iOS ivars in order (_sinVal, _cosVal, _mc, _sensor, _bodies,
// _frameCounter, _frameIndex, _power, _panels, _sound, _frames).
class BoostPanel : public LevelItem
{
public:
    BoostPanel();            // @00587108
    ~BoostPanel() override;  // @0058715c (D1), @005871bc (D0)

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;  // @005871e0  vptr+0x18

    // Sensor box on the level body (half size numPanels * segLength/2 x segLength/2, pixels / ptm).
    // init inlines the same code instead of calling it; no callers.
    void createBodies(b2Vec2 position, float angle, unsigned int numPanels, float segLength);  // @00587590
    void setUpSprites(cocos2d::Vec2 position, float angleDegrees, unsigned int numPanels,
                      float segLength);                                    // @005876a0
    void setFrameForSprite(cocos2d::Sprite* sprite, unsigned int frame);  // @005880c8  (no callers)
    // Sound finish callback (inlined into the frameAction lambda): _sound = nullptr.
    void soundStopped();                                                   // @005880f4  (no callers)

    // LevelItem overrides
    void frameAction() override;                                           // @00587dc0  vptr+0x40
    void bodyWillBeDestroyed(b2Body* body) override;                       // @005880fc  vptr+0xb8  (empty)
    void prepareForTrigger() override;                                     // @00588100  vptr+0xe8
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;  // @00588184  vptr+0xd8
    // Remember other->GetBody() (once) / forget it.
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @0058823c  vptr+0x88
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @005883d8  vptr+0x90

protected:
    float _sinVal;                               // +0x94  sin(-angle - pi/2) (DAT_003f5720 is a double)
    float _cosVal;                               // +0x98  cos(-angle - pi/2)
    cocos2d::Node* _mc;                          // +0xa0  container of the panel sprites (LevelItemsNode child)
    b2Fixture* _sensor;                          // +0xa8  sensor on the level body (begin/end contact listener)
    std::vector<b2Body*> _bodies;                // +0xb0  bodies currently on the panel
    int _frameCounter;                           // +0xc8  zeroed by the ctor, never used (iOS ivar)
    int _frameIndex;                             // +0xcc  frame-skip counter of the panel animation
    int _power;                                  // +0xd0  p4 (default 20)
    std::vector<cocos2d::Sprite*> _panels;       // +0xd8  panel sprites (not retained)
    Sound* _sound;                               // +0xf0  "BoostLoop3" loop while bodies are on it
    std::vector<cocos2d::SpriteFrame*> _frames;  // +0xf8  "boostpanel_1.png" .. "boostpanel_4.png" (not retained)
};
