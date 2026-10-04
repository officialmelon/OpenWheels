// ONLINE (PC addition): see Chain.h. Port of com.totaljerkface.game.level.userspecials.Chain.
#include "online/items/Chain.h"

#include <cmath>

#include "DestructionListener.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Session.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(30, [] { return (LevelItem*)new (std::nothrow) Chain(); },
                               FlashSpecialUse::Override);

// link0MC / link1MC frame 1 bounds (Flash px).
const float kLinkW[2] = {9.0f, 3.0f};
const float kLinkH = 15.0f;

std::string linkArt(int kind, int frame)
{
    return std::string(kind == 0 ? "chain_l0_" : "chain_l1_") + std::to_string(frame);
}

}  // namespace

Chain::~Chain()
{
    for (Node* n : _sprites) {
        if (n) n->release();
    }
}

bool Chain::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupBody;
    (void)groupOffset;
    const float x = num(element, "p0", 0.0f);
    const float y = num(element, "p1", 0.0f);
    const float rotDeg = num(element, "p2", 0.0f);
    const bool sleeping = flag(element, "p3", false);
    const bool interactive = flag(element, "p4", true);
    const int linkCount = std::max(2, std::min(40, inum(element, "p5", 20)));
    const float linkScale = std::max(1.0f, std::min(10.0f, num(element, "p6", 1.0f)));
    const float linkAngle = std::max(-10.0f, std::min(10.0f, num(element, "p7", 0.0f)));

    _frameCounter = (int)std::lround(x + y) % 10;
    const float s = 1.0f + (linkScale - 1.0f) / 9.0f * 2.0f;
    const float turnDeg = -(linkAngle / 10.0f * (360.0f / (linkCount * 2)));
    const float rad = 3.14159265f / 180.0f;
    const float base = 90.0f * rad;
    const float turn = turnDeg * rad;
    const float step = (15.0f - 3.0f * 2.0f) * s;
    const float rot = rotDeg * rad;
    const float c = std::cos(rot), sn = std::sin(rot);
    // Flash ref body transform (px, y down).
    auto toFlash = [&](float lx, float ly) { return b2Vec2(x + lx * c - ly * sn, y + lx * sn + ly * c); };

    Node* layer = flashBackgroundLayer();
    b2World* world = getWorld();
    float px = 0.0f, py = 0.0f;
    std::vector<b2Vec2> points;  // Flash _points after each link (joint anchors), Flash px
    for (int i = 0; i < linkCount; i++) {
        const int kind = i % 2;
        const float a = base + ((i + 1) * turn - turn * 0.5f);
        const float dx = std::cos(a) * step;
        const float dy = std::sin(a) * step;
        const b2Vec2 centerPx = toFlash(px + dx * 0.5f, py + dy * 0.5f);
        const float flashAngleRad = rot + a - base;

        Node* sprite = misc::artOr(linkArt(kind, 1), kLinkW[kind], kLinkH, Color4F(0.35f, 0.35f, 0.38f, 1.0f));
        const float spriteScale = sprite->getScale() * s;
        sprite->setScale(spriteScale);
        sprite->retain();
        if (layer) layer->addChild(sprite, kind);  // Flash: every link1 above every link0
        misc::placeFlash(sprite, centerPx.x, centerPx.y, flashAngleRad / rad);
        _sprites.push_back(sprite);
        _linkKinds.push_back(kind);
        _linkScales.push_back(spriteScale);

        px += dx;
        py += dy;
        if (!interactive) continue;

        b2BodyDef bd;
        bd.type = b2_dynamicBody;
        bd.position = flashToWorld(centerPx.x, centerPx.y);
        bd.angle = -flashAngleRad;
        bd.awake = !sleeping;
        bd.userData = sprite;
        b2Body* body = world->CreateBody(&bd);
        b2PolygonShape box;
        box.SetAsBox(s * kLinkW[kind] * 0.5f / kFlashPtm, s * kLinkH * 0.5f / kFlashPtm);
        b2FixtureDef fd;
        fd.shape = &box;
        fd.density = 17.0f;
        fd.friction = 0.3f;
        fd.restitution = 0.1f;
        fd.filter.categoryBits = 8;
        body->CreateFixture(&fd);
        if (i == 0) _linkMass = body->GetMass();
        getLevel()->addToPaintBody(body);
        _bodies.push_back(body);
        points.push_back(b2Vec2(px, py));
    }
    if (!interactive) return true;

    const float ratio = _linkMass / 0.58752f;
    _breakLimit *= ratio * ratio;
    getLevel()->addToActions(this);

    for (size_t i = 1; i < _bodies.size(); i++) {
        const b2Vec2 anchorPx = toFlash(points[i - 1].x, points[i - 1].y);
        b2RevoluteJointDef jd;
        jd.Initialize(_bodies[i - 1], _bodies[i], flashToWorld(anchorPx.x, anchorPx.y));
        auto* joint = (b2RevoluteJoint*)world->CreateJoint(&jd);
        getSession()->getDestructionListener()->addJointListener(joint, this);
        _joints.push_back(joint);
    }
    return true;
}

void Chain::actions()
{
    const int frames = _clock.advance(getTimeStepOverFlashTimeStep());
    for (int f = 0; f < frames; f++) {
        if (++_frameCounter <= 10) continue;
        _frameCounter = 0;
        // Flash GetReactionForce is a force; the same load gives the same force at 60 Hz.
        const float invDt = getTimeStepInverse();
        for (size_t i = 0; i < _joints.size(); i++) {
            b2RevoluteJoint* joint = _joints[i];
            if (!joint) continue;
            if (joint->GetReactionForce(invDt).LengthSquared() > _breakLimit) {
                breakJoint(i);
                return;
            }
            const b2Vec2 d = joint->GetAnchorB() - joint->GetAnchorA();
            if (d.LengthSquared() > 0.15f) breakJoint(i);
        }
    }
}

void Chain::breakJoint(size_t index)
{
    b2RevoluteJoint* joint = _joints[index];
    b2Body* a = joint->GetBodyA();
    b2Body* b = joint->GetBodyB();
    showBrokenLink(joint->GetAnchorA(), a);
    showBrokenLink(joint->GetAnchorB(), b);
    _joints[index] = nullptr;
    getSession()->getDestructionListener()->removeJointListener(this, joint);
    getWorld()->DestroyJoint(joint);
    const int n = (int)std::ceil(misc::rand01() * 3.0f);
    createBodySound("ChainBreak" + std::to_string(std::max(1, std::min(3, n))), a, 1.0f, false);
}

void Chain::showBrokenLink(const b2Vec2& worldAnchor, b2Body* body)
{
    size_t i = 0;
    while (i < _bodies.size() && _bodies[i] != body) i++;
    if (i == _bodies.size()) return;
    auto* sprite = dynamic_cast<Sprite*>(_sprites[i]);
    if (!sprite) return;
    // Flash local y > 0 (below the centre) = world local y < 0.
    if (body->GetLocalPoint(worldAnchor).y < 0.0f) sprite->setScaleY(-sprite->getScaleY());
    const int frame = 1 + (int)std::ceil(misc::rand01() * 3.0f);
    misc::setFlashFrame(sprite, linkArt(_linkKinds[i], std::max(2, std::min(4, frame))));
}

b2Body* Chain::getJointBody(b2Vec2 point)
{
    b2Body* best = nullptr;
    float bestD = 10000000.0f;
    for (b2Body* body : _bodies) {
        const float d = (point - body->GetWorldCenter()).LengthSquared();
        if (d < bestD) {
            bestD = d;
            best = body;
        }
    }
    return best;
}

void Chain::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    (void)trigger;
    if (_bodies.empty()) return;
    if (action == 0) {
        _bodies[0]->SetAwake(true);
    } else if (action == 1) {
        for (b2Body* body : _bodies) misc::applyFlashImpulse(body, properties, false, _linkMass);
    }
}

void Chain::jointWillBeDestroyed(b2Joint* joint)
{
    for (auto& j : _joints) {
        if (j == joint) j = nullptr;
    }
}

}  // namespace online
