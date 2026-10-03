#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <vector>

class LevelDataElement;

// Level item type 9 (LevelB2D::addSpecial -> FinishLine::create). The finish: a 6.4 x 0.64 m box on
// the level body (begin/end contact counter), pole + cap + 20 banner sprites and an animated flag
// ("flag_frame_1..18.png", advanced every other frame). While something touches it, actions()
// checks whether the character's focus body is inside (_lowerBounds, _upperBounds); if so (and the
// character is alive) it removes its listeners, calls LevelB2D::levelCompleted() and adds two
// sparkle flows at _sparkCoords. A "characterDead" listener (characterDead) disables it.
//
// XML attributes: p0 x, p1 y.
//
// arm64 sizeof 0xf8. The constructor is user-provided but inline (create() inlines it: no zero-fill;
// every member below is initialised). Out-of-line destructor (removes the listener, frees _frames).
// Members are the iOS ivars in order, plus the listener.
class FinishLine : public LevelItem
{
public:
    // Inline: inlined into create(), no symbol.
    FinishLine() {}
    ~FinishLine() override;  // @005b3e10 (D2), @005b3e78 (D0)

    // new(nothrow) FinishLine; init (virtual call); autorelease or delete. Inline in the header in
    // the original: the only copy is emitted in LevelB2D's TU right after LevelB2D::addSpecial.
    static FinishLine* create(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset);  // @005d06d4
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;        // @005b33ac  vptr+0x18

    // Solid 3.2 x 0.32 (half extents) box on the level body (category 8); bounds: upper = (x+3.2,
    // y+3.2), lower = (x-3.2, y);
    // _sparkCoords = (x*ptm - 3.136*ptm, y*ptm + 3.68*ptm).
    void createBody(b2Vec2 position);  // @005b3674
    void createSprites();              // @005b37f8
    // Listener callback: if the character is dead, removeListeners() (inlined copy).
    void characterDead();              // @005b3e9c
    void removeListeners();            // @005b3f14  (no callers)

    // LevelItem overrides
    void actions() override;           // @005b3f70  vptr+0x30
    void frameAction() override;       // @005b40ac  vptr+0x40  flag animation
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;  // @005b4118  vptr+0x88  ++_contactCount
    void endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;    // @005b4128  vptr+0x90  --_contactCount

protected:
    cocos2d::Node* _mc = nullptr;                       // +0x98  container (LevelItemsNode child)
    b2Vec2 _upperBounds = b2Vec2_zero;                  // +0xa0
    b2Vec2 _lowerBounds = b2Vec2_zero;                  // +0xa8
    b2Fixture* _shape = nullptr;                        // +0xb0  begin/end contact listener
    cocos2d::Vec2 _sparkCoords = cocos2d::Vec2::ZERO;   // +0xb8  pixels
    cocos2d::Sprite* _flagSprite = nullptr;             // +0xc0  "flag_frame_1.png", scale 1.5
    std::vector<cocos2d::SpriteFrame*> _frames;         // +0xc8  "flag_frame_1.png".."flag_frame_18.png"
    int _frameIndex = 0;                                // +0xe0  init: 1
    int _contactCount = 0;                              // +0xe4
    bool _updateAnimation = false;                      // +0xe8  init: true; toggled every frame
    cocos2d::EventListenerCustom* _characterDeadListener = nullptr;  // +0xf0  "characterDead", retained
};
