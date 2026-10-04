// ONLINE (PC addition): browser special 11, Meteor (Flash userspecials/Meteor + editor MeteorRef).
// The Android game has no meteor.
//
// XML (MeteorRef._attributes): p0 x, p1 y, p2 shapeWidth (diameter, 0.5..1.5 x 400 px),
// p3 shapeHeight (unused), p4 immovable2, p5 sleeping.
// A heavy rock (density 75) drawn with MeteorMC; immovable ones are a circle on the level body.
// MeteorImpact sound on hard hits. Trigger actions: 0 wake, 1 apply impulse (with spin).

#include "online/items/FlashSpecials.h"
#include "online/items/MiscItemUtil.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"

USING_NS_CC;

namespace online {

namespace {

class Meteor : public FlashItem
{
public:
    ~Meteor() override
    {
        if (_mc) _mc->release();
    }

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override
    {
        (void)groupBody;
        (void)groupOffset;
        const float x = num(element, "p0", 0.0f);
        const float y = num(element, "p1", 0.0f);
        const float d = std::max(200.0f, std::min(600.0f, num(element, "p2", 400.0f)));
        const bool immovable = flag(element, "p4", false);
        const bool sleeping = flag(element, "p5", false);

        if (Sprite* s = createFlashSprite("meteor")) {
            // mc.width = 407.1 * d / 400 for a 407.1 px symbol: scale d / 400.
            s->setScale(s->getScale() * d / 400.0f);
            _mc = s;
        } else {
            auto* dn = DrawNode::create();
            dn->drawSolidCircle(Vec2::ZERO, d * 0.5f * pointsPerFlashPx(), 0, 40, Color4F(0.35f, 0.27f, 0.21f, 1.0f));
            _mc = dn;
        }
        _mc->retain();
        if (Node* layer = flashBackgroundLayer()) layer->addChild(_mc);
        misc::placeFlash(_mc, x, y, 0.0f);

        b2CircleShape circle;
        circle.m_radius = d * 0.5f / kFlashPtm;
        b2FixtureDef fd;
        fd.shape = &circle;
        fd.density = 75.0f;
        fd.friction = 0.5f;
        fd.restitution = 0.1f;
        fd.filter.categoryBits = 8;
        if (!immovable) {
            b2BodyDef bd;
            bd.type = b2_dynamicBody;
            bd.position = flashToWorld(x, y);
            bd.awake = !sleeping;
            bd.userData = _mc;
            _body = getWorld()->CreateBody(&bd);
            addToBeginContact(_body->CreateFixture(&fd));
            getLevel()->addToPaintBody(_body);
        } else {
            circle.m_p = flashToWorld(x, y) - getLevelBody()->GetPosition();
            getLevelBody()->CreateFixture(&fd);
        }
        return true;
    }

    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override
    {
        (void)fixture;
        if (!_body || otherFixture->IsSensor()) return;
        const float m = otherFixture->GetBody()->GetMass();
        if (m != 0.0f && m < _body->GetMass()) return;
        if (misc::normalSpeed(contact) > 2.0f) createBodySound("MeteorImpact", _body, 1.0f, false);
    }

    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override
    {
        (void)trigger;
        if (!_body) return;
        if (action == 0) _body->SetAwake(true);
        else if (action == 1) misc::applyFlashImpulse(_body, properties, true, _body->GetMass());
    }

    std::vector<b2Body*> getBodyList() override
    {
        std::vector<b2Body*> v;
        if (_body) v.push_back(_body);
        return v;
    }

private:
    Node* _mc = nullptr;
    b2Body* _body = nullptr;
};

FlashSpecialRegistration s_reg(11, [] { return (LevelItem*)new (std::nothrow) Meteor(); });

}  // namespace

}  // namespace online
