#pragma once
// ONLINE (PC addition): browser special 22, Boombox (Flash userspecials/Boombox + editor
// BoomboxRef). A 48 x 28 px box (density 2): an impulse > 6 dents it (random damaged frame,
// "BoomboxCrush1", debris), > 17 smashes it in two halves ("BoomboxSmash"). Hits make
// "BoomboxHit". Non-interactive boomboxes are art only and may sit in groups.
//
// XML (BoomboxRef._attributes): p0 x, p1 y, p2 angle, p3 sleeping, p4 interactive.

#include "online/items/props/PropItem.h"

namespace online {

class Boombox : public PropItem
{
public:
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;
    void singleAction() override;

protected:
    void onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse) override;
    void onHit(b2Fixture* fixture) override;

private:
    void showFrame(int frame);
    void shatter();

    b2Fixture* _shape = nullptr;
    cocos2d::Node* _frameArt = nullptr;
    float _crackImpulse = 6.0f;
    float _shatterImpulse = 17.0f;
    bool _break = false;
};

}  // namespace online
