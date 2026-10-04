#pragma once
// ONLINE (PC addition): browser special 1, Table (Flash userspecials/Table + editor TableRef).
// A 156 px table (top + two legs, density 5) that loses its legs on an impulse > 15 ("TableBreak2")
// and then snaps into two halves on > 7 ("TableBreak1"). Non-interactive tables are art only and
// may sit in groups.
//
// XML (TableRef._attributes): p0 x, p1 y, p2 angle, p3 sleeping, p4 interactive.

#include "online/items/props/PropItem.h"

namespace online {

class Table : public PropItem
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
    cocos2d::Node* _intact = nullptr;   // TableMC (top + legs)
    cocos2d::Node* _topOnly = nullptr;  // TableMC.inner without the legs
    bool _break2 = false;
    float _breakLimit = 50.0f;
};

}  // namespace online
