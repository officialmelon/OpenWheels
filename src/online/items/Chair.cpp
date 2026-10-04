// ONLINE (PC addition): see Chair.h. Port of com.totaljerkface.game.level.userspecials.Chair.
#include "online/items/Chair.h"

#include "cocos2d.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "online/FlashRuntime.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(19, [] { return (LevelItem*)new (std::nothrow) Chair(); },
                               FlashSpecialUse::Missing, /*groupable*/ true);

const Color4F kWood(0.80f, 0.78f, 0.60f, 1.0f);

PropFixture chairFixture()
{
    PropFixture f;
    f.density = 5.0f;
    f.friction = 0.1f;
    f.restitution = 0.2f;
    return f;
}

}  // namespace

bool Chair::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupOffset;
    placeRoot(element, groupBody);
    _sign = flag(element, "p3", false) ? -1 : 1;
    const bool interactive = !_inGroup && flag(element, "p5", true);

    // ChairMC: container at y -7.9, mirrored (container.scaleX *= sign).
    Node* container = Node::create();
    container->setCascadeOpacityEnabled(true);
    container->setPosition(pt(0, -7.9f));
    container->setScaleX((float)_sign);
    container->addChild(art("props_chair", 42, 88, 0, 0, kWood));
    _root->addChild(container);
    if (!interactive) return true;

    const float x = num(element, "p0", 0.0f), y = num(element, "p1", 0.0f);
    _body = createBody(flashToWorld(x, y), flashAngle(num(element, "p2", 0.0f)), flag(element, "p4", false));
    const PropFixture def = chairFixture();
    _top = addBox(_body, 21, 3, 0, -3, def);
    _leftLeg = addBox(_body, 3, 16, -18, 16, def);
    _rightLeg = addBox(_body, 3, 16, 18, 16, def);
    _back = addBox(_body, 3, 25, -18.0f * _sign, -31, def);
    paintBody(_body, _root);  // Flash Chair.paint: mc at the world centre
    listenImpulse(_top);
    listenImpulse(_leftLeg);
    listenImpulse(_rightLeg);
    listenImpulse(_back);
    getLevel()->addToActions(this);
    return true;
}

void Chair::onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse)
{
    (void)fixture;
    (void)other;
    (void)contact;
    if (impulse > _breakLimit) {
        forgetImpulse(_top);
        forgetImpulse(_leftLeg);
        forgetImpulse(_rightLeg);
        forgetImpulse(_back);
        getLevel()->addToSingleActions(this);
    }
}

void Chair::singleAction()
{
    if (!_body) return;
    const b2Vec2 center = _body->GetWorldCenter();
    const float spin = _body->GetAngularVelocity();
    const float angle = _body->GetAngle();
    const PropFixture def = chairFixture();
    Node* layer = _root->getParent();

    struct Piece {
        float hw, hh, cx, cy;
        const char* art;
        int scaleX;
    };
    // Order of creation and art as in Chair.actions: legs, back, top.
    const Piece pieces[4] = {
        {3, 16, -18, 16, "props_chair_leg", 1},
        {3, 16, 18, 16, "props_chair_leg", 1},
        {3, 25, -18.0f * _sign, -31, "props_chair_back", 1},
        {21, 3, 0, -3, "props_chair_top", _sign},
    };
    b2Body* top = nullptr;
    for (const Piece& p : pieces) {
        // Placed at the chair's world centre with the shape offsets kept (Flash).
        b2Body* body = createBody(center, angle);
        addBox(body, p.hw, p.hh, p.cx, p.cy, def);
        // Flash: the old body's velocity at the piece's local centre taken as an old-body point.
        body->SetLinearVelocity(_body->GetLinearVelocityFromLocalPoint(body->GetLocalCenter()));
        body->SetAngularVelocity(spin);
        Node* a = art(p.art, p.hw * 2, p.hh * 2, 0, 0, kWood);
        a->setScaleX(a->getScaleX() * p.scaleX);
        if (layer) layer->addChild(a);
        paintBody(body, a);
        top = body;
    }
    playSound("TableBreak1", top, true);
    unpaintBody(_body);
    destroyBody(_body);
    _body = nullptr;
    _root->removeFromParent();
}

}  // namespace online
