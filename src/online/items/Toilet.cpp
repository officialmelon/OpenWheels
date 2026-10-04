// ONLINE (PC addition): see Toilet.h. Port of com.totaljerkface.game.level.userspecials.Toilet.
#include "online/items/Toilet.h"

#include <cmath>

#include "cocos2d.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "online/FlashRuntime.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(24, [] { return (LevelItem*)new (std::nothrow) Toilet(); },
                               FlashSpecialUse::Missing, /*groupable*/ true);

const Color4F kPorcelain(0.95f, 0.95f, 0.90f, 1.0f);

int ceilRandom(int n) { return std::max(1, (int)std::ceil(CCRANDOM_0_1() * n)); }

}  // namespace

std::vector<b2Vec2> Toilet::bowl(bool broken) const
{
    // Toilet.addBowlShapeToPolyDef (its vertex reordering for reversed toilets only restores
    // the winding; Box2D's hull takes care of that here).
    const float s = (float)_sign;
    std::vector<b2Vec2> v;
    v.push_back(b2Vec2(s * -25, 4.5f));
    if (broken) {
        v.push_back(b2Vec2(s * 2, 4.5f));
        v.push_back(b2Vec2(s * 17, 25));
    } else {
        v.push_back(b2Vec2(s * 31, 4.5f));
        v.push_back(b2Vec2(s * 25, 20));
    }
    v.push_back(b2Vec2(s * 3.5f, 30.5f));
    v.push_back(b2Vec2(s * -22, 17));
    return v;
}

void Toilet::centreArt()
{
    // mc.container at -localCenter (the mc itself is painted at the world centre).
    const b2Vec2 lc = _body->GetLocalCenter();
    _container->setPosition(Vec2(-lc.x, -lc.y) * getPtm());
}

Node* Toilet::pieceArt(const char* name, float w, float h)
{
    Node* a = art(name, w, h, 0, 0, kPorcelain);
    a->setScaleX(a->getScaleX() * _sign);
    if (Node* layer = _root->getParent()) layer->addChild(a);
    return a;
}

bool Toilet::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupOffset;
    placeRoot(element, groupBody);
    _sign = flag(element, "p3", false) ? -1 : 1;

    // ToiletMC.container (mirrored): base at (3.4, 20.4), back (tank) at (-20.45, -15.9).
    _container = Node::create();
    _container->setCascadeOpacityEnabled(true);
    _container->setScaleX((float)_sign);
    _baseArt = art("props_toilet_base", 56, 38, 0, 0, kPorcelain);
    _baseArt->setPosition(_baseArt->getPosition() + pt(3.4f, 20.4f));
    _container->addChild(_baseArt);
    _backArt = art("props_toilet_back", 22, 47, 0, 0, kPorcelain);
    _backArt->setPosition(_backArt->getPosition() + pt(-20.45f, -15.9f));
    _container->addChild(_backArt);
    _root->addChild(_container);
    if (_inGroup || !flag(element, "p5", true)) return true;

    const float x = num(element, "p0", 0.0f), y = num(element, "p1", 0.0f);
    _body = createBody(flashToWorld(x, y), flashAngle(num(element, "p2", 0.0f)), flag(element, "p4", false));
    PropFixture def;
    _backShape = addBox(_body, 10, 23.5f, _sign * -21.0f, -16, def);
    def.density = 4.0f;
    _baseShape = addBox(_body, 10.5f, 7.5f, _sign * 3.0f, 31.5f, def);
    def.density = 2.0f;
    _bowlShape = addPolygon(_body, bowl(false), def);
    paintBody(_body, _root);
    centreArt();
    listenImpulse(_backShape);
    listenImpulse(_bowlShape);
    listenImpulse(_baseShape);
    listenHits(_backShape);
    listenHits(_bowlShape);
    listenHits(_baseShape);
    getLevel()->addToActions(this);
    return true;
}

void Toilet::onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse)
{
    (void)other;
    (void)contact;
    if (impulse <= _impulseMin) return;
    if (fixture == _backShape) {
        _breakBack = true;
        forgetImpulse(_backShape);
    } else {
        _breakBase = true;
        forgetImpulse(_bowlShape);
        forgetImpulse(_baseShape);
    }
    getLevel()->addToSingleActions(this);
}

void Toilet::onHit(b2Fixture* fixture)
{
    (void)fixture;
    playGatedSound(1, "ToiletHit" + std::to_string(ceilRandom(2)), _body);
}

void Toilet::singleAction()
{
    if (!_body) return;
    const b2Vec2 center = _body->GetWorldCenter();
    const b2Vec2 velocity = _body->GetLinearVelocity();
    const float spin = _body->GetAngularVelocity();
    const float angle = _body->GetAngle();
    PropFixture def;  // the shared b2PolygonDef ends up with density 2
    const bool fwd = _sign == 1;
    const float s = (float)_sign;

    if (_breakBase && _baseShape) {
        destroyFixture(_baseShape);
        destroyFixture(_bowlShape);
        _baseShape = _bowlShape = nullptr;

        // Pieces sit at the old world centre with the original shape offsets (Flash).
        b2Body* piece = createBody(center, angle);
        addBox(piece, 10.5f, 7.5f, s * 3.0f, 31.5f, def);
        addPolygon(piece, bowl(true), def);
        piece->SetAngularVelocity(spin);
        piece->SetLinearVelocity(velocity);
        Node* a = pieceArt("props_toilet_basepiece1", 42, 39);
        paintBody(piece, a);
        rectBurst("props_toiletpart_", 5, 20, piece, 30);
        spray("props_water_", 13, piece, fwd ? 11.0f : -1.0f, fwd ? 22.0f : 13.0f, fwd ? 1.0f : -11.0f,
              fwd ? 13.0f : 22.0f, 0, 2, 15, 3, 95);
        bringToFront(a);  // the spray goes in below the piece

        piece = createBody(center, angle);
        addPolygon(piece, {b2Vec2(s * 8, 4.5f), b2Vec2(s * 31, 4.5f), b2Vec2(s * 25, 20), b2Vec2(s * 17, 25)}, def);
        piece->SetAngularVelocity(spin);
        piece->SetLinearVelocity(velocity);
        paintBody(piece, pieceArt("props_toilet_basepiece2", 26, 22));
        _baseArt->setVisible(false);
        playGatedSound(0, "ToiletSmash" + std::to_string(ceilRandom(3)), piece, true);
    }
    if (_breakBack && _backShape) {
        destroyFixture(_backShape);
        _backShape = nullptr;
        b2Body* piece = createBody(center, angle);
        addBox(piece, 10, 12.5f, s * -14.0f, -31, def);
        piece->SetAngularVelocity(spin);
        piece->SetLinearVelocity(velocity);
        paintBody(piece, pieceArt("props_toilet_backpiece1", 22, 28));

        piece = createBody(center, angle);
        addBox(piece, 10, 11, s * -14.0f, -7, def);
        Node* a = pieceArt("props_toilet_backpiece2", 20, 24);
        paintBody(piece, a);
        piece->SetAngularVelocity(spin);
        piece->SetLinearVelocity(velocity);
        _backArt->setVisible(false);
        rectBurst("props_toiletpart_", 5, 20, piece, 30);
        spray("props_water_", 13, piece, fwd ? -7.0f : 23.0f, fwd ? -14.0f : -13.0f, fwd ? -23.0f : 7.0f,
              fwd ? -13.0f : -14.0f, 0, 1, 15, 3, 80);
        bringToFront(a);
        playGatedSound(0, "ToiletSmash" + std::to_string(ceilRandom(3)), piece, true);
    }
    if (_breakBase && _breakBack) {
        unpaintBody(_body);
        destroyBody(_body);
        _body = nullptr;
        _root->removeFromParent();
        return;
    }
    _body->ResetMassData();
    centreArt();
}

}  // namespace online
