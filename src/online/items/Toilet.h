#pragma once
// ONLINE (PC addition): browser special 24, Toilet (Flash userspecials/Toilet + editor ToiletRef).
// Tank (back), bowl and base on one body. An impulse > 12 on the tank breaks it off in two
// pieces, > 12 on the bowl / base breaks those off in two pieces; each break sprays water and
// porcelain ("ToiletSmash"), hits make "ToiletHit". Non-interactive toilets are art only and may
// sit in groups.
//
// XML (ToiletRef._attributes): p0 x, p1 y, p2 angle, p3 reverse, p4 sleeping, p5 interactive.

#include "online/items/props/PropItem.h"

namespace online {

class Toilet : public PropItem
{
public:
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;
    void singleAction() override;

protected:
    void onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse) override;
    void onHit(b2Fixture* fixture) override;

private:
    std::vector<b2Vec2> bowl(bool broken) const;
    void centreArt();
    cocos2d::Node* pieceArt(const char* name, float w, float h);

    b2Fixture* _backShape = nullptr;
    b2Fixture* _bowlShape = nullptr;
    b2Fixture* _baseShape = nullptr;
    cocos2d::Node* _container = nullptr;
    cocos2d::Node* _baseArt = nullptr;
    cocos2d::Node* _backArt = nullptr;
    int _sign = 1;
    float _impulseMin = 12.0f;
    bool _breakBack = false;
    bool _breakBase = false;
};

}  // namespace online
