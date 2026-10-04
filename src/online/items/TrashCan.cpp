// ONLINE (PC addition): see TrashCan.h. Port of com.totaljerkface.game.level.userspecials.TrashCan.
#include "online/items/TrashCan.h"

#include <cmath>

#include "cocos2d.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "online/FlashRuntime.h"

USING_NS_CC;

namespace online {

namespace {

FlashSpecialRegistration s_reg(26, [] { return (LevelItem*)new (std::nothrow) TrashCan(); },
                               FlashSpecialUse::Missing, /*groupable*/ true);

const Color4F kMetal(0.70f, 0.72f, 0.72f, 1.0f);
const Color4F kTrash(0.55f, 0.50f, 0.40f, 1.0f);

// TrashCanMC.shapes children (Flash px; width / height = clip bounds x placement scale).
const float kBlockW = 44.7986f, kBlockH = 74.2768f;           // shapes.block
const float kS1X = 0.0f, kS1Y = 14.7f, kS1W = 45.3300f, kS1H = 41.2631f;  // hollow bottom
const float kBlockerW = 45.3300f, kBlockerH = 70.7309f;       // shapes.blocker
const float kCrushHW = 27.1872f, kCrushHH = 70.4986f;         // shapes.crushH
const float kCrushVW = 45.1106f, kCrushVH = 35.1503f;         // shapes.crushV
const b2Vec2 kLeft[3] = {b2Vec2(-22.65f, -35.55f), b2Vec2(-12.65f, 24.45f), b2Vec2(-22.65f, 35.45f)};
const b2Vec2 kRight[3] = {b2Vec2(22.65f, -35.55f), b2Vec2(22.65f, 35.45f), b2Vec2(12.65f, 24.45f)};

struct TrashItem {
    const char* art;  // props_trash_<c|r><n>_<frame>
    int frames;
    float x, y, w, h;
    bool circle;
};
// shapes.c0..c2 (CircleTrashItem0..2) then shapes.r0..r6 (RectTrashItem0..6; r6 is the lid).
const TrashItem kItems[10] = {
    {"props_trash_c0_", 2, -8.95f, -28.8f, 12.5839f, 12.5839f, true},
    {"props_trash_c1_", 3, 10.0f, -13.05f, 9.375f, 9.375f, true},
    {"props_trash_c2_", 1, -15.9f, -32.55f, 9.375f, 9.375f, true},
    {"props_trash_r0_", 2, -8.7f, -18.8f, 6.7855f, 21.6971f, false},
    {"props_trash_r1_", 1, -13.65f, -19.0f, 5.2755f, 23.3056f, false},
    {"props_trash_r2_", 1, 6.4f, -31.5f, 18.0837f, 7.1890f, false},
    {"props_trash_r3_", 2, 15.45f, -27.9f, 5.2758f, 9.2626f, false},
    {"props_trash_r4_", 2, 5.9f, -20.35f, 16.9280f, 13.3060f, false},
    {"props_trash_r5_", 2, -0.75f, -13.35f, 12.6198f, 9.5527f, false},
    {"props_trash_r6_", 1, 0.05f, -36.2f, 47.1403f, 4.7218f, false},
};

PropFixture canFixture()
{
    PropFixture f;
    f.density = 0.75f;
    return f;
}

int ceilRandom(int n) { return std::max(1, (int)std::ceil(CCRANDOM_0_1() * n)); }

}  // namespace

void TrashCan::showFrame(int frame)
{
    if (_canArt) _canArt->removeFromParent();
    if (frame == 1) {
        _canArt = art("props_trashcan_1", 53, 71, 0, 0, kMetal);
    } else if (frame == 2) {
        _canArt = art("props_trashcan_2", kCrushHW, kCrushHH, 0, 0, kMetal);
    } else {
        _canArt = art("props_trashcan_3", kCrushVW, kCrushVH, 0, 0, kMetal);
    }
    _root->addChild(_canArt, -1);
    _canShifted = false;
}

bool TrashCan::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    (void)groupOffset;
    const bool interactive = !groupBody && flag(element, "p4", true);
    _containsTrash = flag(element, "p5", true);
    placeRoot(element, groupBody, /*foreground*/ interactive);  // foreground.addChildAt(mc, 0)
    showFrame(1);
    _lidArt = art("props_trash_r6_1", 48, 5, 0, 0, kMetal);
    _lidArt->setPosition(_lidArt->getPosition() + pt(0, -35.4f));
    _lidArt->setVisible(_containsTrash);
    _root->addChild(_lidArt);
    if (!interactive) return true;

    const float x = num(element, "p0", 0.0f), y = num(element, "p1", 0.0f);
    const b2Vec2 position = flashToWorld(x, y);
    const float angle = flashAngle(num(element, "p2", 0.0f));
    getLevel()->addToActions(this);
    if (!_containsTrash) {
        // Flash creates the full box, destroys it again and builds the (awake) hollow can.
        createHollowTrashCan(position, angle, b2Vec2_zero, 0.0f);
        _spillImpulse = _crushImpulse;
        listenImpulse(_shape);
        listenImpulse(_leftShape);
        listenImpulse(_rightShape);
        return true;
    }
    _body = createBody(position, angle, flag(element, "p3", false));
    _shape = addBox(_body, kBlockW / 2, kBlockH / 2, 0, 0, canFixture());
    paintBody(_body, _root);
    listenImpulse(_shape);
    listenHits(_shape);
    return true;
}

void TrashCan::createHollowTrashCan(const b2Vec2& position, float angle, const b2Vec2& velocity, float spin)
{
    _body = createBody(position, angle);
    _body->SetAngularVelocity(spin);
    _body->SetLinearVelocity(velocity);
    const PropFixture def = canFixture();
    _shape = addBox(_body, kS1W / 2, kS1H / 2, kS1X, kS1Y, def);
    _leftShape = addPolygon(_body, {kLeft[0], kLeft[1], kLeft[2]}, def);
    _rightShape = addPolygon(_body, {kRight[0], kRight[1], kRight[2]}, def);
    paintBody(_body, _root);
    if (!_canShifted) {
        _canShifted = true;
        _canArt->setPosition(_canArt->getPosition() + pt(0, -13));  // mc.can.y -= 13
    }
    listenHits(_shape);
    listenHits(_leftShape);
    listenHits(_rightShape);
}

void TrashCan::onImpulse(b2Fixture* fixture, b2Fixture* other, b2Contact* contact, float impulse)
{
    (void)fixture;
    (void)other;
    if (impulse <= _spillImpulse) return;
    if (impulse > _crushImpulse) _crush = true;
    _spillImpulse = _crushImpulse;
    if (_contactImpulse < impulse) {
        _contactImpulse = impulse;
        b2WorldManifold wm;
        contact->GetWorldManifold(&wm);
        _contactNormal = wm.normal;
    }
    forgetImpulse(_shape);  // Flash only deletes the main shape's RESULT listener
    getLevel()->addToSingleActions(this);
}

void TrashCan::onHit(b2Fixture* fixture)
{
    (void)fixture;
    playGatedSound(1, "TrashCanHit" + std::to_string(ceilRandom(2)), _body);
}

void TrashCan::singleAction()
{
    if (!_body) return;
    const b2Vec2 position = _body->GetPosition();
    const float angle = _body->GetAngle();
    const b2Vec2 velocity = _body->GetLinearVelocity();
    const float spin = _body->GetAngularVelocity();
    unpaintBody(_body);
    destroyBody(_body);
    _body = nullptr;
    _shape = _leftShape = _rightShape = nullptr;
    _lidArt->setVisible(false);

    if (_crush) {
        // Hit along the can's long side -> narrow (crushH), else flat (crushV). The y flip only
        // changes the sign of sin(), which is taken absolute.
        const float a = std::atan2(_contactNormal.y, _contactNormal.x) - angle;
        const bool horizontal = std::fabs(std::round(std::sin(a))) != 1.0f;
        _body = createBody(position, angle);
        _body->SetAngularVelocity(spin);
        _body->SetLinearVelocity(velocity);
        showFrame(horizontal ? 2 : 3);
        // background.addChild(mc): out of the foreground.
        if (Node* bg = flashBackgroundLayer()) {
            _root->removeFromParent();
            bg->addChild(_root);
        }
        if (horizontal) {
            _shape = addBox(_body, kCrushHW / 2, kCrushHH / 2, 0, 0, canFixture());
        } else {
            _shape = addBox(_body, kCrushVW / 2, kCrushVH / 2, 0, 0, canFixture());
        }
        paintBody(_body, _root);
        listenHits(_shape);
    } else {
        createHollowTrashCan(position, angle, velocity, spin);
        listenImpulse(_shape);
        listenImpulse(_leftShape);
        listenImpulse(_rightShape);
    }

    if (!_trashSpilled && _containsTrash) {
        _trashSpilled = true;
        Node* bg = flashBackgroundLayer();
        const PropFixture def = canFixture();
        for (const TrashItem& t : kItems) {
            const b2Vec2 p = _body->GetWorldPoint(propLocal(t.x, t.y));
            b2Body* body = createBody(p, _body->GetAngle());
            if (t.circle) {
                addCircle(body, t.w / 2, def);
            } else {
                addBox(body, t.w / 2, t.h / 2, 0, 0, def);
            }
            body->SetAngularVelocity(spin);
            body->SetLinearVelocity(velocity);
            const int frame = ceilRandom(t.frames);
            Node* a = art(std::string(t.art) + std::to_string(frame), t.w, t.h, 0, 0, kTrash);
            if (bg) bg->addChild(a);
            paintBody(body, a);
        }
        if (!_crush) {
            // shapes.blocker: massless, only meets category 2.
            PropFixture blocker;
            blocker.density = 0.0f;
            blocker.friction = 0.0f;
            blocker.restitution = 0.1f;
            blocker.category = 2;
            blocker.mask = 2;
            addBox(_body, kBlockerW / 2, kBlockerH / 2, 0, 0, blocker);
        }
    }
    playSound("TrashCanSpill" + std::to_string(ceilRandom(2)), _body);
}

}  // namespace online
