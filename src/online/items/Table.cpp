// ONLINE (PC addition): see Table.h. Port of com.totaljerkface.game.level.userspecials.Table.
#include "online/items/Table.h"

#include "cocos2d.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "online/FlashRuntime.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(1, [] { return (LevelItem*)new (std::nothrow) Table(); },
                               FlashSpecialUse::Missing, /*groupable*/ true);

const Color4F kWood(0.80f, 0.78f, 0.60f, 1.0f);

PropFixture tableFixture()
{
    PropFixture f;
    f.density = 5.0f;
    f.friction = 0.1f;
    f.restitution = 0.2f;
    return f;
}

}  // namespace

bool Table::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupOffset;
    placeRoot(element, groupBody);
    const bool interactive = !_inGroup && flag(element, "p4", true);

    // TableMC: inner (top + leg1 + leg2) at y -3; createBody moves inner to -11.
    _intact = art("props_table", 156, 56, 0, 22, kWood);
    _root->addChild(_intact);
    _topOnly = art("props_table_top", 156, 6, 0, 0, kWood);
    _topOnly->setVisible(false);
    _root->addChild(_topOnly);
    if (!interactive) return true;

    // Session.version (the game's) is never < 1.67 in v1.87: interactive tables break at 15.
    _breakLimit = 15.0f;
    const float x = num(element, "p0", 0.0f), y = num(element, "p1", 0.0f);
    _body = createBody(flashToWorld(x, y), flashAngle(num(element, "p2", 0.0f)), flag(element, "p3", false));
    const PropFixture def = tableFixture();
    _top = addBox(_body, 78, 3, 0, -3, def);
    _leftLeg = addBox(_body, 3, 25, -48, 25, def);
    _rightLeg = addBox(_body, 3, 25, 48, 25, def);
    _intact->setPosition(_intact->getPosition() + pt(0, -8));  // mc.inner.y = -11
    paintBody(_body, _root);
    listenImpulse(_top);
    listenImpulse(_leftLeg);
    listenImpulse(_rightLeg);
    getLevel()->addToActions(this);
    return true;
}

void Table::onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse)
{
    (void)fixture;
    (void)other;
    (void)contact;
    if (impulse > _breakLimit) {
        forgetImpulse(_top);
        forgetImpulse(_leftLeg);
        forgetImpulse(_rightLeg);
        getLevel()->addToSingleActions(this);  // Flash: actionsVector, removed again on its first run
    }
}

void Table::singleAction()
{
    if (!_body) return;
    LevelB2D* level = getLevel();
    const b2Vec2 center = _body->GetWorldCenter();
    const b2Vec2 velocity = _body->GetLinearVelocity();
    const float spin = _body->GetAngularVelocity();
    const float angle = _body->GetAngle();
    const PropFixture def = tableFixture();
    Node* layer = _root->getParent();

    if (!_break2) {
        listenImpulse(_top);
        destroyFixture(_leftLeg);
        destroyFixture(_rightLeg);
        _leftLeg = _rightLeg = nullptr;
        _body->ResetMassData();
        _intact->setVisible(false);  // mc.inner.leg1/leg2 hidden, mc.inner.y = 0
        _topOnly->setVisible(true);
        // The legs keep their offsets but are placed at the table's old world centre (Flash).
        for (int side = -1; side <= 1; side += 2) {
            b2Body* leg = createBody(center, angle);
            addBox(leg, 3, 25, side * 48.0f, 25, def);
            leg->SetLinearVelocity(velocity);
            leg->SetAngularVelocity(spin);
            Node* legArt = art("props_table_leg", 6, 50, 0, 0, kWood);
            if (layer) layer->addChild(legArt);
            paintBody(leg, legArt);
        }
        _break2 = true;
        _breakLimit = 7.0f;
        playSound("TableBreak2", _body);
        return;
    }

    unpaintBody(_body);
    destroyBody(_body);
    _body = nullptr;
    _top = nullptr;
    b2Body* first = nullptr;
    for (int side = -1; side <= 1; side += 2) {
        b2Body* half = createBody(center, angle);
        addBox(half, 39, 3, side * 39.0f, -3, def);
        half->SetLinearVelocity(velocity);
        half->SetAngularVelocity(spin);
        Node* halfArt = art(side < 0 ? "props_table_top1" : "props_table_top2", 78, 6, 0, 0, kWood);
        if (layer) layer->addChild(halfArt);
        paintBody(half, halfArt);
        if (!first) first = half;
    }
    _root->removeFromParent();
    playSound("TableBreak1", first, true);
}

}  // namespace online
