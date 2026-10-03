#pragma once

#include "LevelItem.h"

#include "cocos2d.h"
#include "Box2D/Box2D.h"

#include <vector>

class LevelDataElement;

// Level item type 3 (LevelB2D::addSpecial -> IBeam::create). A steel I-beam: one "ibeam.png" sprite
// scaled to width x height, a box fixture on its own body, on the group body (when it is part of a
// group) or on the level body (fixed). Contacts play "IBeamHit<1|2>" (handleContactAdds).
//
// XML attributes: p0 x, p1 y, p2 width, p3 height, p4 angle (deg), p5 fixed, p6 sleeping.
// (The decompiler hides the attribute names: same "p0".."p6" short strings as Log.)
//
// arm64 sizeof 0xb8. The constructor is user-provided but inline (create() inlines it: no zero-fill,
// _body/_fixture/_mc = nullptr, _size = Size(6.45161295f, 0.512f)). Destructor trivial (vtable
// slot 0 is LevelItem's D1). Members are the iOS ivars in order.
class IBeam : public LevelItem
{
public:
    // Inline: inlined into create(), no symbol.
    IBeam() {}
    ~IBeam() override;  // @005c8b8c (D0; D1 is LevelItem's)

    // new(nothrow) IBeam; init (virtual call); autorelease or delete. Inline in the header in the
    // original: the only copy is emitted in LevelB2D's TU right after LevelB2D::addSpecial.
    static IBeam* create(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset);  // @005d08e8
    // _body = groupBody first (createBody then adds the fixture to it instead of making a body).
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;   // @005c8198  vptr+0x18

    void createBody(b2Vec2 position, float angle, float width, float height, bool fixed,
                    bool sleeping);                                   // @005c85f0

    // LevelItem overrides
    void actions() override;                                          // @005c884c  vptr+0x30  -> handleContactAdds()
    b2Body* getJointBody(b2Vec2 point) override;                      // @005c89c8  vptr+0x50  _body
    void handleContactAdds() override;                                // @005c89d0  vptr+0xc0
    // Group items are painted by their GroupItem: _mc->setRotation(angle), setPosition(offset).
    void paintWithOffsetPoints(cocos2d::Vec2 offset, float angleDegrees) override;  // @005c87dc  vptr+0xc8
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;  // @005c8858  vptr+0xd8
    std::vector<b2Body*> getBodyList() override;                      // @005c8b48  vptr+0xf0

protected:
    b2Body* _body = nullptr;          // +0x98  own body, the group body, or nullptr (fixed)
    b2Fixture* _fixture = nullptr;    // +0xa0  begin-contact listener (not when fixed)
    cocos2d::Node* _mc = nullptr;     // +0xa8  container of the "ibeam.png" sprite (body user data)
    // Size of the "ibeam.png" art in metres: sprite scale = (width / w + width / w) * 1.01, height / h.
    cocos2d::Size _size = cocos2d::Size(6.45161295f, 0.512f);  // +0xb0
};
