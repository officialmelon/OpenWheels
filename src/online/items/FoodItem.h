#pragma once
// ONLINE (PC addition): browser special 32, FoodItem (Flash userspecials/FoodItem + FoodChunk,
// editor FoodItemRef): watermelon (1), pumpkin (2) or pineapple (3). One polygon (density 2,
// material 4 = food, shape userData = the item) that splats on an impulse > 8 / 8 / 3 into four
// round chunks (material 1) with juice particles and "FoodSplat". Non-interactive food is art
// only and may sit in groups.
//
// XML (FoodItemRef._attributes): p0 x, p1 y, p2 angle, p3 sleeping, p4 interactive,
// p5 foodItemType (1..3).

#include "online/items/Grindable.h"
#include "online/items/props/PropItem.h"

namespace online {

class FoodItem : public PropItem, public Grindable
{
public:
    // Lawnmower Man blade (Flash FoodItem.grindShape; the mower sprays food particles).
    void grindFixture(b2Fixture* fixture) override;
    void grindBody(b2Body* body) override;
    int grindSprayType() const override { return _type; }
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;
    void singleAction() override;

protected:
    void onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse) override;

private:
    int _type = 1;
    b2Fixture* _shape = nullptr;
    bool _willSmash = false;
};

}  // namespace online
