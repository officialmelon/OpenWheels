// ONLINE (PC addition): see FoodItem.h. Port of com.totaljerkface.game.level.userspecials.FoodItem
// and FoodChunk.
#include "online/items/FoodItem.h"

#include <algorithm>
#include <cmath>

#include "cocos2d.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "online/FlashRuntime.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(32, [] { return (LevelItem*)new (std::nothrow) FoodItem(); },
                               FlashSpecialUse::Missing, /*groupable*/ true);

struct Chunk {
    float x, y, radius;  // mc.chunks.m<i> position, mc.chunks.c<i> width / 2 (Flash px)
};

struct Food {
    std::vector<b2Vec2> shape;  // mc.shapes.p0_<i>
    Chunk chunks[4];
    float smashImpulse;          // _initialSmashImpulse
    int particleFrames;          // <Food>ParticlesMC
    float w, h;                  // fallback size
    Color4F color;
};

const Food& food(int type)
{
    static const Food foods[3] = {
        {{b2Vec2(-12.0f, -12.4f), b2Vec2(9.15f, -12.7f), b2Vec2(18.1f, 1.5f), b2Vec2(7.65f, 13.3f),
          b2Vec2(-12.5f, 13.0f), b2Vec2(-20.2f, 1.05f)},
         {{1.9f, 1.0f, 8.4642f}, {11.9f, -4.2f, 7.2792f}, {3.35f, 8.25f, 6.6306f}, {-5.65f, -2.9f, 10.8516f}},
         8.0f, 15, 40, 27, Color4F(0.25f, 0.55f, 0.20f, 1.0f)},
        {{b2Vec2(-11.9f, -12.85f), b2Vec2(12.25f, -14.05f), b2Vec2(20.05f, 0.2f), b2Vec2(13.0f, 13.05f),
          b2Vec2(-9.1f, 14.05f), b2Vec2(-20.1f, 0.6f)},
         {{0.5f, -13.15f, 7.1008f}, {13.5f, -0.5f, 6.7868f}, {2.4f, 3.05f, 8.3788f}, {-10.7f, 1.6f, 8.4718f}},
         8.0f, 9, 40, 28, Color4F(0.95f, 0.50f, 0.15f, 1.0f)},
        {{b2Vec2(-6.85f, -16.95f), b2Vec2(5.65f, -17.2f), b2Vec2(6.45f, 20.1f), b2Vec2(-6.85f, 19.7f)},
         {{-0.6f, -8.0f, 7.1008f}, {5.0f, 7.05f, 3.6640f}, {0.6f, 14.3f, 5.7736f}, {-1.95f, 10.45f, 5.7736f}},
         3.0f, 8, 14, 37, Color4F(0.85f, 0.75f, 0.25f, 1.0f)},
    };
    return foods[std::max(1, std::min(3, type)) - 1];
}

}  // namespace

bool FoodItem::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupOffset;
    _type = std::max(1, std::min(3, inum(element, "p5", 1)));
    const Food& f = food(_type);
    placeRoot(element, groupBody);
    _root->addChild(art("props_food_" + std::to_string(_type), f.w, f.h, 0, 0, f.color));
    if (_inGroup || !flag(element, "p4", true)) return true;

    const float x = num(element, "p0", 0.0f), y = num(element, "p1", 0.0f);
    _body = createBody(flashToWorld(x, y), flashAngle(num(element, "p2", 0.0f)), flag(element, "p3", false));
    PropFixture def;  // density 2, friction 0.3, restitution 0.1, category 8
    _shape = addPolygon(_body, f.shape, def);
    _shape->SetUserData(this);                    // SetUserData(this)
    getLevel()->addFixtureMaterial(_shape, 4);     // SetMaterial(4)
    listenImpulse(_shape);
    registerGrindBody(_body);  // Lawnmower Man blade
    paintBody(_body, _root);
    getLevel()->addToActions(this);
    return true;
}

void FoodItem::onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse)
{
    (void)fixture;
    (void)other;
    (void)contact;
    if (impulse > food(_type).smashImpulse) {
        _willSmash = true;
        getLevel()->addToSingleActions(this);
        forgetImpulse(_shape);
    }
}

void FoodItem::singleAction()
{
    if (!_willSmash || !_body) return;
    const Food& f = food(_type);
    Node* bg = flashBackgroundLayer();
    // FoodChunk: a circle body at the chunk clip's place, with the food's velocity.
    for (int i = 0; i < 4; i++) {
        const Chunk& c = f.chunks[i];
        b2Body* body = createBody(_body->GetWorldPoint(propLocal(c.x, c.y)), _body->GetAngle());
        body->SetLinearVelocity(_body->GetLinearVelocity());
        body->SetAngularVelocity(_body->GetAngularVelocity());
        PropFixture def;
        b2Fixture* fixture = addCircle(body, c.radius, def);
        fixture->SetUserData(this);
        getLevel()->addFixtureMaterial(fixture, 1);
        Node* a = art("props_food_" + std::to_string(_type) + "_m" + std::to_string(i), c.radius * 2,
                      c.radius * 2, 0, 0, f.color);
        if (bg) bg->addChild(a);
        paintBody(body, a);
    }
    const int splat = std::max(1, (int)std::ceil(CCRANDOM_0_1() * 3));
    playSound("FoodSplat" + std::to_string(splat), _body);
    rectBurst("props_foodpart_" + std::to_string(_type) + "_", f.particleFrames, 10, _body, 30);
    unpaintBody(_body);
    unregisterGrindBody(_body);
    destroyBody(_body);
    _body = nullptr;
    _shape = nullptr;
    _root->removeFromParent();
}

void FoodItem::grindFixture(b2Fixture* fixture)
{
    // Flash grindShape: no splat any more, contact listeners off.
    (void)fixture;
    _willSmash = false;
    if (_shape) forgetImpulse(_shape);
}

void FoodItem::grindBody(b2Body* body)
{
    // The mower deactivates the body (kept, not destroyed); the art goes with it.
    if (body != _body) return;
    unregisterGrindBody(body);
    unpaintBody(body);
    if (_root) _root->setVisible(false);
    _body = nullptr;
    _shape = nullptr;
}

}  // namespace online
