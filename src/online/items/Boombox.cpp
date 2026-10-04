// ONLINE (PC addition): see Boombox.h. Port of com.totaljerkface.game.level.userspecials.Boombox.
#include "online/items/Boombox.h"

#include <cmath>

#include "cocos2d.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "online/FlashRuntime.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(22, [] { return (LevelItem*)new (std::nothrow) Boombox(); },
                               FlashSpecialUse::Missing, /*groupable*/ true);

const Color4F kGrey(0.75f, 0.75f, 0.75f, 1.0f);

int ceilRandom(int n) { return std::max(1, (int)std::ceil(CCRANDOM_0_1() * n)); }

}  // namespace

void Boombox::showFrame(int frame)
{
    if (_frameArt) _frameArt->removeFromParent();
    _frameArt = art("props_boombox_" + std::to_string(frame), 48, 28, 0, 0, kGrey);
    _root->addChild(_frameArt);
}

bool Boombox::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupOffset;
    placeRoot(element, groupBody);
    showFrame(1);
    if (_inGroup || !flag(element, "p4", true)) return true;

    const float x = num(element, "p0", 0.0f), y = num(element, "p1", 0.0f);
    _body = createBody(flashToWorld(x, y), flashAngle(num(element, "p2", 0.0f)), flag(element, "p3", false));
    PropFixture def;
    _shape = addBox(_body, 24, 14, 0, 0, def);
    paintBody(_body, _root);
    listenImpulse(_shape);
    listenHits(_shape);
    getLevel()->addToActions(this);
    return true;
}

void Boombox::onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse)
{
    (void)fixture;
    (void)other;
    (void)contact;
    if (impulse > _crackImpulse) {
        if (impulse > _shatterImpulse) {
            forgetHits(_shape);
            _break = true;
        }
        _crackImpulse = _shatterImpulse;
        forgetImpulse(_shape);
        getLevel()->addToSingleActions(this);
    }
}

void Boombox::onHit(b2Fixture* fixture)
{
    (void)fixture;
    playSound("BoomboxHit", _body);
}

void Boombox::singleAction()
{
    if (!_body) return;
    if (!_break) {
        listenImpulse(_shape);
        playSound("BoomboxCrush1", _body);
        rectBurst("props_boomboxshard_", 6, 8, _body, 15);
        showFrame(1 + ceilRandom(3));
        _break = true;
        return;
    }
    shatter();
}

void Boombox::shatter()
{
    rectBurst("props_boomboxshard_", 6, 10, _body, 30);
    playSound("BoomboxSmash" + std::to_string(ceilRandom(2)), _body);
    const float spin = _body->GetAngularVelocity();
    const float angle = _body->GetAngle();
    Node* layer = _root->getParent();
    const float xs[2] = {-13.6f, 13.55f};
    PropFixture def;
    for (int i = 0; i < 2; i++) {
        b2Body* piece = createBody(_body->GetWorldPoint(propLocal(xs[i], 1.8f)), angle);
        addBox(piece, 10.5f, 14, 0, 0, def);
        piece->SetAngularVelocity(spin);
        piece->SetLinearVelocity(_body->GetLinearVelocityFromLocalPoint(piece->GetLocalCenter()));
        Node* a = art("props_boombox_piece" + std::to_string(i + 1), 21, 28, 0, 0, kGrey);
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
