#pragma once
// ONLINE (PC addition): browser special 21, TV (Flash userspecials/TV + editor TVRef).
// A 55 x 40 px box (density 2): an impulse > 7 cracks the screen (random cracked frame,
// "GlassLight", shards), > 20 shatters it into three triangular pieces ("TVSmash"). Hits make
// "TVHit". Non-interactive TVs are art only and may sit in groups.
//
// XML (TVRef._attributes): p0 x, p1 y, p2 angle, p3 sleeping, p4 interactive.

#include "online/items/props/PropItem.h"

namespace online {

class TV : public PropItem
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
    float _crackImpulse = 7.0f;
    float _shatterImpulse = 20.0f;
    bool _break = false;
};

}  // namespace online
