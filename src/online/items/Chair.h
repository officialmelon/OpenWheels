#pragma once
// ONLINE (PC addition): browser special 19, Chair (Flash userspecials/Chair + editor ChairRef).
// Seat, two legs and a back on one body (density 5); an impulse > 10 breaks it into four pieces
// ("TableBreak1"). Non-interactive chairs are art only and may sit in groups.
//
// XML (ChairRef._attributes): p0 x, p1 y, p2 angle, p3 reverse, p4 sleeping, p5 interactive.

#include "online/items/props/PropItem.h"

namespace online {

class Chair : public PropItem
{
public:
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;
    void singleAction() override;

protected:
    void onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse) override;

private:
    b2Fixture* _top = nullptr;
    b2Fixture* _leftLeg = nullptr;
    b2Fixture* _rightLeg = nullptr;
    b2Fixture* _back = nullptr;
    int _sign = 1;
    float _breakLimit = 10.0f;
};

}  // namespace online
