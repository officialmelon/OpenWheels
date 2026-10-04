// ONLINE (PC addition): see TV.h. Port of com.totaljerkface.game.level.userspecials.TV.
#include "online/items/TV.h"

#include <cmath>

#include "cocos2d.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "online/FlashRuntime.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(21, [] { return (LevelItem*)new (std::nothrow) TV(); },
                               FlashSpecialUse::Missing, /*groupable*/ true);

const Color4F kGrey(0.55f, 0.55f, 0.50f, 1.0f);

int ceilRandom(int n) { return std::max(1, (int)std::ceil(CCRANDOM_0_1() * n)); }

}  // namespace

void TV::showFrame(int frame)
{
    if (_frameArt) _frameArt->removeFromParent();
    _frameArt = art("props_tv_" + std::to_string(frame), 55, 40, 0, 0, kGrey);
    _root->addChild(_frameArt);
}

bool TV::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupOffset;
    placeRoot(element, groupBody);
    showFrame(1);
    if (_inGroup || !flag(element, "p4", true)) return true;

    const float x = num(element, "p0", 0.0f), y = num(element, "p1", 0.0f);
    _body = createBody(flashToWorld(x, y), flashAngle(num(element, "p2", 0.0f)), flag(element, "p3", false));
    PropFixture def;  // density 2, friction 0.3, restitution 0.1, category 8
    _shape = addBox(_body, 27.5f, 20, 0, 0, def);
    paintBody(_body, _root);
    listenImpulse(_shape);
    listenHits(_shape);
    getLevel()->addToActions(this);
    return true;
}

void TV::onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse)
{
    (void)fixture;
    (void)other;
    (void)contact;
    if (impulse > _crackImpulse) {
        if (impulse > _shatterImpulse) _break = true;
        _crackImpulse = _shatterImpulse;
        forgetImpulse(_shape);
        getLevel()->addToSingleActions(this);
    }
}

void TV::onHit(b2Fixture* fixture)
{
    (void)fixture;
    playSound("TVHit", _body);
}

void TV::singleAction()
{
    if (!_body) return;
    if (!_break) {
        listenImpulse(_shape);
        playSound("GlassLight" + std::to_string(ceilRandom(2)), _body);
        rectBurst("props_tvshard_", 9, 10, _body, 30);
        showFrame(1 + ceilRandom(4));
        _break = true;
        return;
    }
    forgetHits(_shape);
    shatter();
}

void TV::shatter()
{
    rectBurst("props_tvshard_", 9, 10, _body, 60);
    playSound("TVSmash" + std::to_string(ceilRandom(2)), _body);
    const b2Vec2 center = _body->GetWorldCenter();
    const float spin = _body->GetAngularVelocity();
    const float angle = _body->GetAngle();
    Node* layer = _root->getParent();
    // TV.shatter: three triangles (Flash px), placed at the old world centre.
    const float tris[3][6] = {
        {-26, -18, 0, 20, -26, 20},
        {26, -15, 25, 20, -2, 20},
        {-24, -18, 25, -18, -1, 4},
    };
    PropFixture def;
    for (int i = 0; i < 3; i++) {
        b2Body* piece = createBody(center, angle);
        addPolygon(piece, {b2Vec2(tris[i][0], tris[i][1]), b2Vec2(tris[i][2], tris[i][3]),
                           b2Vec2(tris[i][4], tris[i][5])}, def);
        piece->SetAngularVelocity(spin);
        piece->SetLinearVelocity(_body->GetLinearVelocityFromLocalPoint(piece->GetLocalCenter()));
        Node* a = art("props_tv_piece" + std::to_string(i + 1), 26, 38, 0, 0, kGrey);
        if (layer) layer->addChild(a);
        paintBody(piece, a);
    }
    unpaintBody(_body);
    destroyBody(_body);
    _body = nullptr;
    _shape = nullptr;
    _root->removeFromParent();
}

}  // namespace online
