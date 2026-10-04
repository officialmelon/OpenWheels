#pragma once
// ONLINE (PC addition): browser special 26, TrashCan (Flash userspecials/TrashCan + editor
// TrashCanRef). A light box (density 0.75) drawn in front of the characters while interactive.
// An impulse > 3.5 knocks the lid off and spills the trash (10 small bodies) leaving a hollow can
// ("TrashCanSpill"); > 12 crushes it flat or narrow depending on the hit direction. Hits make
// "TrashCanHit". Non-interactive cans are art only and may sit in groups.
//
// XML (TrashCanRef._attributes): p0 x, p1 y, p2 angle, p3 sleeping, p4 interactive,
// p5 containsTrash.

#include "online/items/props/PropItem.h"

namespace online {

class TrashCan : public PropItem
{
public:
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;
    void singleAction() override;

protected:
    void onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse) override;
    void onHit(b2Fixture* fixture) override;

private:
    void createHollowTrashCan(const b2Vec2& position, float angle, const b2Vec2& velocity, float spin);
    void showFrame(int frame);

    b2Fixture* _shape = nullptr;
    b2Fixture* _leftShape = nullptr;
    b2Fixture* _rightShape = nullptr;
    cocos2d::Node* _canArt = nullptr;
    cocos2d::Node* _lidArt = nullptr;
    bool _crush = false;
    bool _containsTrash = true;
    bool _trashSpilled = false;
    bool _canShifted = false;
    float _spillImpulse = 3.5f;
    float _crushImpulse = 12.0f;
    float _contactImpulse = -1.0f;  // strongest qualifying contact (Flash _contact)
    b2Vec2 _contactNormal = b2Vec2(0.0f, 1.0f);
};

}  // namespace online
