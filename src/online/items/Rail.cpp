// ONLINE (PC addition): browser special 27, Rail (Flash userspecials/Rail + editor RailRef).
// The Android game has no rail.
//
// XML (RailRef._attributes): p0 x, p1 y, p2 shapeWidth 100..2000, p3 shapeHeight (fixed), p4 angle.
// A static grind rail: RailMC stretched to the width, two half-height boxes (18 px total) on a
// static body, the upper one with material 4 as in Flash. Not jointable, no trigger actions.

#include "online/items/FlashSpecials.h"
#include "online/items/MiscItemUtil.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"

USING_NS_CC;

namespace online {

namespace {

class Rail : public FlashItem
{
public:
    ~Rail() override
    {
        if (_mc) _mc->release();
    }

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override
    {
        (void)groupBody;
        (void)groupOffset;
        const float x = num(element, "p0", 0.0f);
        const float y = num(element, "p1", 0.0f);
        const float w = std::max(100.0f, std::min(2000.0f, num(element, "p2", 250.0f)));
        const float angle = num(element, "p4", 0.0f);
        const float h = 18.0f;

        if (Sprite* s = createFlashSprite("rail")) {
            s->setScaleX(s->getScaleX() * w / 100.0f);  // RailMC is 100 px wide
            _mc = s;
        } else {
            auto* d = DrawNode::create();
            const float k = pointsPerFlashPx();
            d->drawSolidRect(Vec2(-w * 0.5f * k, -h * 0.5f * k), Vec2(w * 0.5f * k, h * 0.5f * k),
                             Color4F(0.6f, 0.6f, 0.62f, 1.0f));
            _mc = d;
        }
        _mc->retain();
        if (Node* layer = flashBackgroundLayer()) layer->addChild(_mc);
        misc::placeFlash(_mc, x, y, angle);

        b2BodyDef bd;
        bd.position = flashToWorld(x, y);
        bd.angle = flashAngle(angle);
        b2Body* body = getWorld()->CreateBody(&bd);
        b2PolygonShape box;
        b2FixtureDef fd;
        fd.shape = &box;
        fd.friction = 0.3f;
        fd.restitution = 0.1f;
        fd.filter.categoryBits = 8;
        // Flash local y -h/4 (upper half) = world local +h/4.
        box.SetAsBox(w / 2 / kFlashPtm, h / 4 / kFlashPtm, b2Vec2(0.0f, h / 4 / kFlashPtm), 0.0f);
        getLevel()->addFixtureMaterial(body->CreateFixture(&fd), 4);
        box.SetAsBox(w / 2 / kFlashPtm, h / 4 / kFlashPtm, b2Vec2(0.0f, -h / 4 / kFlashPtm), 0.0f);
        body->CreateFixture(&fd);
        return true;
    }

private:
    Node* _mc = nullptr;
};

FlashSpecialRegistration s_reg(27, [] { return (LevelItem*)new (std::nothrow) Rail(); });

}  // namespace

}  // namespace online
