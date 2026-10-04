#pragma once
// ONLINE (PC addition): things the restored Lawnmower Man's blade grinds besides characters.
//
// Flash LawnMowerMan catches any shape whose material has bit 1, 2 or 4 and then tells the
// shape's owner (shape userData): NPCharacter.grindShape / removeBody, CharacterB2D.grindShape /
// removeBody, FoodItem.grindShape; food sprays its own particles instead of blood. The browser
// items register their bodies here so the mower can find the owner by body:
//
//     if (online::Grindable* g = online::Grindable::forBody(body)) {
//         g->grindFixture(fixture);           // caught by the blade (Flash grindShape)
//         ... later: g->grindBody(body);       // finished (Flash removeBody)
//         g->grindSprayType();                 // 0 blood, 1..3 food particle type
//     }
//
// Bodies unregister themselves when destroyed by their owner.

class b2Body;
class b2Fixture;

namespace online {

class Grindable
{
public:
    virtual ~Grindable();

    // The body was caught by the blade: stop smashing / bleeding from it (Flash grindShape).
    virtual void grindFixture(b2Fixture* fixture) = 0;
    // The blade finished the body; the mower then deactivates and hides it (Flash removeBody).
    virtual void grindBody(b2Body* body) = 0;
    // What the blade sprays: 0 blood, else a FoodItem particle type (1 watermelon, 2 pumpkin,
    // 3 pineapple).
    virtual int grindSprayType() const { return 0; }

    static Grindable* forBody(b2Body* body);

protected:
    void registerGrindBody(b2Body* body);
    void unregisterGrindBody(b2Body* body);
};

}  // namespace online
