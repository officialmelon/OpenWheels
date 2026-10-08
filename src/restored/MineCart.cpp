// RESTORED (PC addition): Explorer Guy's mine cart, ported from the browser game's
// MiddleAgedExplorer.as (Flash v1.87) onto the mobile Vehicle framework. See MineCart.h.

#include "MineCart.h"

#include <algorithm>
#include <cmath>

#include "CharacterB2D.h"
#include "ExplorerGuy.h"
#include "FlashParticles.h"
#include "LevelB2D.h"
#include "Patch.h"
#include "Session.h"
#include "Settings.h"
#include "Sound.h"
#include "platform/compat/Box2DFloat.h"
#include "online/FlashPhysics.h"  // RESTORED (PC addition): per-step constants at 1/30 too

USING_NS_CC;

namespace {

const float kFlashPx = 1.0f / 62.5f;  // metres per Flash world px (Flash m_physScale 62.5)
const float kDeg = 0.017453292f;

void limitJoint(b2RevoluteJoint* joint, b2Body* a, b2Body* b, float lowerDeg, float upperDeg)
{
    if (!joint) {
        return;
    }
    float angle = b->GetAngle() - a->GetAngle();
    joint->SetLimits(-upperDeg * kDeg - angle, -lowerDeg * kDeg - angle);
}

}  // namespace

MineCart* MineCart::create(Vec2 position, std::string name, int groupID)
{
    MineCart* cart = new (std::nothrow) MineCart();
    if (cart && cart->init(position, name, groupID)) {
        cart->autorelease();
        return cart;
    }
    delete cart;
    return nullptr;
}

MineCart::MineCart()
    : _accelStep(0.5f),
      _prisAccelStep(0.25f),
      _wheelMaxSpeed(1000.0f),
      _impulseLeft(0.7f),
      _impulseRight(0.7f),
      _impulseOffset(1.0f),
      _maxSpinAV(5.0f),
      _hatSmashLimit(0.75f),
      _frameSmashLimit(150.0f),
      _railDistanceMin(37.0f * kFlashPx),
      _railJointY(23.0f * kFlashPx),
      _oneDongleMax(70),
      _rider(nullptr),
      _riderEjected(false),
      _connecting(false),
      _frameSmashed(false),
      _oneDongleCounter(0),
      _frameBody(nullptr),
      _bottomFixture(nullptr),
      _leftFixture(nullptr),
      _rightFixture(nullptr),
      _frameSprite(nullptr),
      _frontWheelSprite(nullptr),
      _backWheelSprite(nullptr),
      _rollLoop{nullptr, nullptr, nullptr}
{
    _frontWheelFixture = nullptr;
    _backWheelFixture = nullptr;
}

MineCart::~MineCart()
{
    for (Sound*& s : _rollLoop) {
        if (s) {
            s->setFinishCallback(nullptr);
        }
    }
}

bool MineCart::gameplay()
{
    return getSession()->getMode() == SessionModeGameplay;
}

bool MineCart::init(Vec2 position, std::string name, int groupID)
{
    _maxTorque = 40.0f;
    _maxSpeed = 1000.0f;
    bool ok = Vehicle::init(position, name, groupID);
    if (ok) {
        getLevel()->addToPaintItem(this);
    }
    return ok;
}

// ---- helpers ---------------------------------------------------------------------------------

ValueMap MineCart::shape(const std::string& name)
{
    return _bodiesDict.at("bodies").asValueMap().at(name).asValueMap();
}

b2Vec2 MineCart::point(const std::string& name)
{
    Vec2 p = PointFromString(_bodiesDict.at("joints").asValueMap().at(name).asString()) + _origin;
    return b2Vec2(p.x, p.y);
}

// Bodies sit on the guide origin with angle 0, so guide positions are body-local.
b2Fixture* MineCart::addBox(b2Body* body, b2FixtureDef def, const std::string& name)
{
    ValueMap data = shape(name);
    return createFixture(body, def, &data, true, true);
}

b2Fixture* MineCart::addPolygon(b2Body* body, b2FixtureDef def, const std::string& prefix)
{
    b2Vec2 vertices[4];
    for (int i = 0; i < 4; i++) {
        b2Vec2 p = point(prefix + patch::to_string(i));
        vertices[i] = b2Vec2(p.x - _origin.x, p.y - _origin.y);
    }
    b2PolygonShape polygon;
    polygon.Set(vertices, 4);
    def.shape = &polygon;
    return body->CreateFixture(&def);
}

void MineCart::paintBody(b2Body* body, const std::string& frame)
{
    Sprite* sprite = Sprite::createWithSpriteFrameName(_name + "_" + frame + ".png");
    getSession()->getVehicleForeground()->addChild(sprite, (int)_painted.size());
    _painted.push_back({body, sprite});
}

// ---- creation --------------------------------------------------------------------------------

void MineCart::createSprites()
{
    // Flash puts the cart above the rider's front arm: he stands in it.
    Node* foreground = getSession()->getVehicleForeground();
    _backWheelSprite = Sprite::createWithSpriteFrameName(_name + "_wheel.png");
    foreground->addChild(_backWheelSprite, -3);
    _frontWheelSprite = Sprite::createWithSpriteFrameName(_name + "_wheel.png");
    foreground->addChild(_frontWheelSprite, -2);
    _frameSprite = Sprite::createWithSpriteFrameName(_name + "_frame.png");
    foreground->addChild(_frameSprite, -1);
}

void MineCart::createBodies()
{
    b2World* world = getWorld();
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.allowSleep = false;
    bodyDef.position.Set(_origin.x, _origin.y);

    // Flash: frame density 1.5, friction 0.3, restitution 0.1, category 514.
    b2FixtureDef frame;
    frame.density = 1.5f;
    frame.friction = 0.3f;
    frame.restitution = 0.1f;
    frame.filter.categoryBits = 0x202;
    _frameBody = world->CreateBody(&bodyDef);
    _bottomFixture = addBox(_frameBody, frame, "frameBottomShape");
    _leftFixture = addPolygon(_frameBody, frame, "sidePoint0_");
    _rightFixture = addPolygon(_frameBody, frame, "sidePoint1_");
    _frameBody->ResetMassData();

    // Wheels: density 12, friction 1, restitution 0.3, in the rider's group (category 260,
    // mask 268: they never touch him).
    b2FixtureDef wheel;
    wheel.density = 12.0f;
    wheel.friction = 1.0f;
    wheel.restitution = 0.3f;
    wheel.filter.groupIndex = (int16)_groupID;
    wheel.filter.categoryBits = 0x104;
    wheel.filter.maskBits = 0x10c;
    b2CircleShape circle;
    wheel.shape = &circle;
    for (Clamp* c : {&_front, &_back}) {
        ValueMap data = shape(c == &_front ? "frontWheelShape" : "backWheelShape");
        Vec2 p = PointFromString(data.at("pos").asString());
        c->localPos = b2Vec2(p.x, p.y);
        bodyDef.position.Set(p.x + _origin.x, p.y + _origin.y);
        circle.m_radius = data.at("radius").asFloat();
        c->wheel = world->CreateBody(&bodyDef);
        b2Fixture* f = c->wheel->CreateFixture(&wheel);
        (c == &_front ? _frontWheelFixture : _backWheelFixture) = f;
    }

    for (b2Fixture* f : {_bottomFixture, _leftFixture, _rightFixture, _frontWheelFixture, _backWheelFixture}) {
        addToPostSolve(f);
    }
    _painted.push_back({_frameBody, _frameSprite});
}

void MineCart::createJoints()
{
    b2World* world = getWorld();
    b2RevoluteJointDef def;
    def.maxMotorTorque = _maxTorque;
    for (Clamp* c : {&_front, &_back}) {
        def.Initialize(_frameBody, c->wheel, c->wheel->GetPosition());
        c->wheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&def));
        addWheelJoint(c->wheelJoint, c->wheel);
    }
}

void MineCart::createDictionaries()
{
    for (b2Fixture* f : {_bottomFixture, _leftFixture, _rightFixture}) {
        _contactImpulseDict[f] = _frameSmashLimit;
    }
    _contactAddSounds[_backWheelFixture] = "CarTire1";
    _contactAddSounds[_frontWheelFixture] = "CarTire1";
}

void MineCart::lockWheels()
{
    if (_back.wheelJoint) {
        _back.wheelJoint->EnableLimit(true);
        _back.wheelJoint->SetLimits(0.0f, 0.0f);
    }
}

// Flash MiddleAgedExplorer.createJoints: hands on the rim, feet on the floor.
void MineCart::addCharacter(CharacterB2D* character)
{
    Vehicle::addCharacter(character);
    _rider = character;
    b2World* world = getWorld();
    CharacterB2D* c = character;

    limitJoint(c->getHipJoint1(), c->getPelvisBody(), c->getUpperLeg1Body(), -50.0f, 10.0f);
    limitJoint(c->getHipJoint2(), c->getPelvisBody(), c->getUpperLeg2Body(), -50.0f, 10.0f);
    limitJoint(c->getElbowJoint1(), c->getUpperArm1Body(), c->getLowerArm1Body(), -60.0f, 0.0f);
    limitJoint(c->getElbowJoint2(), c->getUpperArm2Body(), c->getLowerArm2Body(), -60.0f, 0.0f);
    limitJoint(c->getNeckJoint(), c->getChestBody(), c->getHeadBody(), 0.0f, 20.0f);

    b2RevoluteJointDef def;
    def.maxMotorTorque = _maxTorque;
    def.enableLimit = true;
    def.lowerAngle = -15.0f * kDeg;
    def.upperAngle = 15.0f * kDeg;
    def.Initialize(c->getLowerLeg1Body(), _frameBody, point("footAnchor"));
    addBodyVehicleJoint(c->getLowerLeg1Body(), world->CreateJoint(&def));
    def.Initialize(c->getLowerLeg2Body(), _frameBody, point("footAnchor"));
    addBodyVehicleJoint(c->getLowerLeg2Body(), world->CreateJoint(&def));
    def.enableLimit = false;
    def.Initialize(c->getLowerArm1Body(), _frameBody, point("handAnchor"));
    addBodyVehicleJoint(c->getLowerArm1Body(), world->CreateJoint(&def));
    def.Initialize(c->getLowerArm2Body(), _frameBody, point("handAnchor"));
    addBodyVehicleJoint(c->getLowerArm2Body(), world->CreateJoint(&def));
}

// Flash: every head, chest or pelvis smash, torso or neck break throws him out.
void MineCart::handleInjury(CharacterInjury injury, CharacterB2D* character)
{
    // A broken hip or knee shows the leg the cart hid (Flash hip/kneeBreak).
    if (ExplorerGuy* explorer = dynamic_cast<ExplorerGuy*>(character)) {
        if (injury == CharacterInjuryHip1Break || injury == CharacterInjuryKnee1Break) {
            explorer->showLowerLeg(1);
        } else if (injury == CharacterInjuryHip2Break || injury == CharacterInjuryKnee2Break) {
            explorer->showLowerLeg(2);
        }
    }
    if (injury == CharacterInjuryHeadSmash) {
        handleFatalWound(character);
        return;
    }
    Vehicle::handleInjury(injury, character);
}

void MineCart::checkStateOfCharacter(CharacterB2D* character)
{
    auto held = [this](b2Body* body) {
        auto it = _bodyVehicleJointDict.find(body);
        return it != _bodyVehicleJointDict.end() && it->second != nullptr;
    };
    if (!held(character->getLowerArm1Body()) && !held(character->getLowerArm2Body()) &&
        !held(character->getLowerLeg1Body()) && !held(character->getLowerLeg2Body())) {
        ejectCharacter(character);
    }
}

// Flash eject: the cart's shapes get the zero filter in group -2.
bool MineCart::ejectCharacter(CharacterB2D* character)
{
    if (!Vehicle::ejectCharacter(character)) {
        return false;
    }
    _riderEjected = true;
    _ejected = true;
    _connecting = false;
    if (ExplorerGuy* explorer = dynamic_cast<ExplorerGuy*>(character)) {
        explorer->showLowerLeg(0);
    }
    forwardBackButtonsNull();
    leanButtonsNull();
    if (_frameBody) {
        b2Filter filter = _zeroFilter;
        filter.groupIndex = -2;
        for (b2Fixture* f = _frameBody->GetFixtureList(); f; f = f->GetNext()) {
            f->SetFilterData(filter);
        }
    }
    return true;
}

// ---- controls ----------------------------------------------------------------------------------

// Flash accelerateMotorSpeed / decelerateMotorSpeed: a clamped wheel drives its dongle along the
// rail (prismatic motor; positive = forward along the rail), a free one its own motor (Flash
// positive = clockwise = mobile negative). Accelerating has no reverse-stop, decelerating does.
void MineCart::accelerate(Clamp& c, bool forward)
{
    if (!c.wheelJoint) {
        return;
    }
    if (c.dongle && c.railJoint) {
        if (c.wheelJoint->IsMotorEnabled()) {
            c.wheelJoint->EnableMotor(false);
        }
        c.railJoint->EnableMotor(true);
        float speed = c.railJoint->GetJointSpeed();
        float next;
        // RESTORED (PC addition): per 60 Hz step; online::perStep converts to the current step.
        const float prisAccelStep = online::perStep(_prisAccelStep);
        if (forward) {
            next = speed < _wheelMaxSpeed ? speed + prisAccelStep : speed;
        } else {
            next = speed > 0.0f ? 0.0f : (speed > -_wheelMaxSpeed ? speed - prisAccelStep : speed);
        }
        c.railJoint->SetMotorSpeed(next);
        return;
    }
    if (!c.wheelJoint->IsMotorEnabled()) {
        c.wheelJoint->EnableMotor(true);
    }
    float speed = -owb2::jointSpeed(c.wheelJoint);  // Flash sense
    float next;
    const float accelStep = online::perStep(_accelStep);  // RESTORED (PC addition), as above
    if (forward) {
        next = speed < _wheelMaxSpeed ? speed + accelStep : speed;
    } else {
        next = speed > 0.0f ? 0.0f : (speed > -_wheelMaxSpeed ? speed - accelStep : speed);
    }
    c.wheelJoint->SetMotorSpeed(-next);
}

void MineCart::forwardButtonPressed()
{
    if (_riderEjected || _frameSmashed) {
        return;
    }
    accelerate(_front, true);
    accelerate(_back, true);
}

void MineCart::backButtonPressed()
{
    if (_riderEjected || _frameSmashed) {
        return;
    }
    accelerate(_front, false);
    accelerate(_back, false);
}

void MineCart::forwardBackButtonsNull()
{
    for (Clamp* c : {&_front, &_back}) {
        if (c->railJoint) {
            c->railJoint->EnableMotor(false);
        }
        if (c->wheelJoint && c->wheelJoint->IsMotorEnabled()) {
            c->wheelJoint->EnableMotor(false);
        }
    }
    if (_currentPose == VehiclePoseForward || _currentPose == VehiclePoseBack) {
        setCurrentPose(VehiclePoseNone);
    }
}

// Flash leftPressedActions (impulseLeft at local centre + offset), as RoadBike's mobile version.
void MineCart::leanBackButtonPressed()
{
    if (_riderEjected || !_frameBody) {
        return;
    }
    setCurrentPose(VehiclePoseLeanBack);
    float angularVelocity = _frameBody->GetAngularVelocity();
    double angle = _frameBody->GetAngle() + M_PI;
    float factor = b2Min(b2Max((double)((angularVelocity - _maxSpinAV) / -_maxSpinAV), 0.0), 1.0);
    double magnitude = _impulseLeft * s_timeStepOverFlashTimeStep;
    b2Vec2 impulse((float)(sin(angle) * magnitude * factor), -(float)(cos(angle) * magnitude * factor));
    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x + _impulseOffset, localCenter.y)), true);
}

void MineCart::leanForwardButtonPressed()
{
    if (_riderEjected || !_frameBody) {
        return;
    }
    setCurrentPose(VehiclePoseLeanForward);
    float angularVelocity = _frameBody->GetAngularVelocity();
    double angle = _frameBody->GetAngle() + M_PI;
    float factor = b2Min(b2Max((angularVelocity + _maxSpinAV) / _maxSpinAV, 0.0f), 1.0f);
    double magnitude = _impulseRight * s_timeStepOverFlashTimeStep;
    b2Vec2 impulse((float)(sin(angle) * magnitude * factor), -(float)(cos(angle) * magnitude * factor));
    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x - _impulseOffset, localCenter.y)), true);
}

// Space: clamp onto rails while held (Flash spacePressedActions / spaceNullActions).
void MineCart::special1ButtonPressed()
{
    if (_riderEjected || _frameSmashed) {
        return;
    }
    _connecting = true;
}

void MineCart::special1ButtonNull()
{
    _connecting = false;
    bool released = false;
    for (Clamp* c : {&_front, &_back}) {
        if (c->dongle) {
            removeDongle(*c);
            released = true;
        }
    }
    (void)released;  // (the release click is played by checkRemoveRailJoints in Flash)
}

void MineCart::extraControls(unsigned char state)
{
    if (!riderOn()) {
        return;
    }
    if (state & 0x20) {
        setCurrentPose(VehiclePoseExtra1);
    } else if (state & 0x40) {
        setCurrentPose(VehiclePoseExtra2);
    } else if (_currentPose == VehiclePoseExtra1 || _currentPose == VehiclePoseExtra2) {
        setCurrentPose(VehiclePoseNone);
    }
}

// Flash leanForwardPose (shift): legs straight, arms forward. setJoint(j, a, g) -> mobile
// setJoint(j, -a, g, 20) (Flash targets the lower limit, mobile the upper one).
void MineCart::extraPose1()
{
    if (!riderOn()) {
        return;
    }
    CharacterB2D* c = _rider;
    for (b2RevoluteJoint* j : {c->getKneeJoint1(), c->getKneeJoint2(), c->getHipJoint1(), c->getHipJoint2()}) {
        if (j) {
            c->setJoint(j, 0.0f, 10.0f, 20.0f);
        }
    }
    for (b2RevoluteJoint* j : {c->getElbowJoint1(), c->getElbowJoint2()}) {
        if (j) {
            c->setJoint(j, 15.0f * kDeg, 10.0f, 20.0f);
        }
    }
}

// Flash leanBackPose (ctrl): knees bent all the way, hips at 45 degrees.
void MineCart::extraPose2()
{
    if (!riderOn()) {
        return;
    }
    CharacterB2D* c = _rider;
    for (b2RevoluteJoint* j : {c->getKneeJoint1(), c->getKneeJoint2()}) {
        if (j) {
            c->setJoint(j, -45.0f, 10.0f, 20.0f);
        }
    }
    for (b2RevoluteJoint* j : {c->getHipJoint1(), c->getHipJoint2()}) {
        if (j) {
            c->setJoint(j, 45.0f * kDeg, 10.0f, 20.0f);
        }
    }
}

// ---- rails ---------------------------------------------------------------------------------------

// Flash wheelIsCloseToRail: the wheel centre within 37 px of the rail's line and over its length.
bool MineCart::wheelCloseToRail(const Clamp& c)
{
    if (!c.rail || !c.wheel) {
        return false;
    }
    b2Vec2 local = c.rail->GetLocalPoint(c.wheel->GetPosition());
    if (std::fabs(local.y) > _railDistanceMin) {
        return false;
    }
    float halfWidth = 0.0f;
    for (b2Fixture* f = c.rail->GetFixtureList(); f; f = f->GetNext()) {
        if (f->GetShape()->GetType() == b2Shape::e_polygon) {
            b2PolygonShape* p = static_cast<b2PolygonShape*>(f->GetShape());
            for (int i = 0; i < p->m_count; i++) {
                halfWidth = std::max(halfWidth, std::fabs(p->m_vertices[i].x));
            }
        }
    }
    return std::fabs(local.x) <= halfWidth + c.wheel->GetFixtureList()->GetShape()->m_radius;
}

void MineCart::removeDongle(Clamp& c)
{
    if (c.dongle) {
        getWorld()->DestroyBody(c.dongle);  // takes both of its joints
    }
    c.dongle = nullptr;
    c.dongleJoint = nullptr;
    c.railJoint = nullptr;
    c.rail = nullptr;
    _oneDongleCounter = 0;
}

void MineCart::checkRemoveRailJoints()
{
    if (!_front.dongle && !_back.dongle) {
        return;
    }
    if (!_connecting) {
        removeDongle(_front);
        removeDongle(_back);
        if (gameplay() && _frameBody) {
            createBodySound("DoubleClickReverse", _frameBody, 0.6f, false);
        }
        return;
    }
    for (Clamp* c : {&_front, &_back}) {
        if (!c->dongle) {
            continue;
        }
        b2Vec2 drift = c->dongleJoint->GetAnchorB() - c->dongleJoint->GetAnchorA();
        // Flash: squared drift > 0.1 Flash m^2
        if (!wheelCloseToRail(*c) || drift.LengthSquared() > 0.1f) {
            removeDongle(*c);
        }
    }
}

// Flash createDongle / createDongleRevJoint / createPrisJoint: a heavy sensor box on the wheel's
// spot of the frame, turned like the rail, sliding along it 23 px above its centre line.
void MineCart::attach(Clamp& c)
{
    b2World* world = getWorld();
    b2Body* rail = c.newRail;
    c.newRail = nullptr;
    bool fresh = false;
    if (c.dongle) {
        world->DestroyJoint(c.railJoint);
        c.railJoint = nullptr;
    } else {
        b2BodyDef def;
        def.type = b2_dynamicBody;
        def.position = _frameBody->GetWorldPoint(c.localPos);
        def.angle = rail->GetAngle();
        def.fixedRotation = true;
        def.allowSleep = false;
        c.dongle = world->CreateBody(&def);
        b2PolygonShape box;
        box.SetAsBox(0.25f, 0.25f);
        b2FixtureDef fixture;
        fixture.shape = &box;
        fixture.density = 375.0f;
        fixture.isSensor = true;
        c.dongle->CreateFixture(&fixture);
        c.dongle->ResetMassData();

        b2RevoluteJointDef pin;
        pin.Initialize(c.dongle, _frameBody, c.dongle->GetPosition());
        pin.localAnchorA.SetZero();
        pin.localAnchorB = c.localPos;
        pin.maxMotorTorque = 300.0f;
        c.dongleJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&pin));
        fresh = true;
    }

    // Start the dongle at the wheel's speed along the rail (Flash getPrisJointSpeed).
    float railAngle = rail->GetAngle();
    b2Vec2 along(cosf(railAngle), sinf(railAngle));
    float speed = b2Dot(c.wheel->GetLinearVelocity(), along);
    c.dongle->SetLinearVelocity(speed * along);

    b2Vec2 anchorLocal = rail->GetLocalPoint(_frameBody->GetWorldPoint(c.localPos));
    anchorLocal.y = _railJointY;
    b2PrismaticJointDef slide;
    slide.Initialize(c.dongle, rail, rail->GetWorldPoint(anchorLocal), -along);
    slide.localAnchorA.SetZero();
    slide.localAnchorB = anchorLocal;
    slide.maxMotorForce = 1000.0f;
    slide.motorSpeed = speed;
    slide.enableMotor = false;
    c.railJoint = static_cast<b2PrismaticJoint*>(world->CreateJoint(&slide));
    c.rail = rail;

    if (fresh) {
        // Flash addSparkForAttachedWheel: sparks under the wheel, on the rail.
        float r = c.wheel->GetFixtureList()->GetShape()->m_radius;
        b2Vec2 up(-sinf(railAngle), cosf(railAngle));
        b2Vec2 p = c.wheel->GetPosition() - r * up;
        if (restored::FlashParticles* particles = restored::FlashParticles::forSession(getSession())) {
            particles->sparks(p, b2Vec2(0.5f, 0.5f), 0.25f, 25.0f, 20);
        }
    }
}

void MineCart::checkAddRailJoints()
{
    bool attachedNew = false;
    b2Body* soundBody = nullptr;
    for (Clamp* c : {&_front, &_back}) {
        if (!c->newRail) {
            continue;
        }
        bool hadDongle = c->dongle != nullptr;
        attach(*c);
        if (!hadDongle) {
            attachedNew = true;
            soundBody = c->dongle;
        }
    }
    if (attachedNew && gameplay() && soundBody) {
        createBodySound("DoubleClick", soundBody, 0.6f, false);
    }
}

// ---- per frame -----------------------------------------------------------------------------------

void MineCart::actions()
{
    if (_frameBody) {
        checkRemoveRailJoints();
        if (!_riderEjected) {
            checkAddRailJoints();
        }
    }
    // The dongles do not fall (Flash: + GRAVITY_DISPLACEMENT per frame).
    b2Vec2 lift = -getWorld()->GetGravity();
    lift *= LevelItem::s_timeStep;  // RESTORED (PC addition): one step (was 1/60)
    for (Clamp* c : {&_front, &_back}) {
        if (c->dongle) {
            c->dongle->SetLinearVelocity(c->dongle->GetLinearVelocity() + lift);
        }
    }
    if ((_front.dongle != nullptr) != (_back.dongle != nullptr)) {
        // RESTORED (PC addition): _oneDongleMax counts 60 Hz steps; converted to the current step.
        if (++_oneDongleCounter == online::stepsFor60HzFrames(_oneDongleMax)) {
            removeDongle(_front.dongle ? _front : _back);
        }
    }
    wheelSounds();

    // Vehicle::actions without its single tyre loop (the cart has its own three).
    checkPose();
    checkJoints();
    handleContactAdds();
    handleContactResults();
}

void MineCart::startLoop(Sound*& sound, const std::string& name)
{
    if (sound || !gameplay() || !_back.wheel) {
        return;
    }
    sound = createBodySound(name, _back.wheel, 1.0f, true);
    if (sound) {
        Sound** slot = &sound;
        sound->setFinishCallback([slot](int&) { *slot = nullptr; });
        sound->setMaxVolume(0.0f);
        sound->fadeTo(1.0f, 0.2f, false);
    }
}

void MineCart::stopLoop(Sound*& sound)
{
    if (sound) {
        sound->fadeTo(0.0f, 0.2f, true);
        sound->setFinishCallback(nullptr);
        sound = nullptr;
    }
}

// Flash actions: ExplorerRoll1/2/3 by the back wheel's spin (> 5, > 25, > 50 rad/s).
void MineCart::wheelSounds()
{
    int level = -1;
    if (_wheelContacts > 0 && _back.wheel) {
        float av = std::fabs(_back.wheel->GetAngularVelocity());
        level = av > 50.0f ? 2 : av > 25.0f ? 1 : av > 5.0f ? 0 : -1;
    }
    for (int i = 0; i < 3; i++) {
        if (i == level) {
            startLoop(_rollLoop[i], "ExplorerRoll" + patch::to_string(i + 1));
        } else {
            stopLoop(_rollLoop[i]);
        }
    }
}

void MineCart::paint()
{
    float ptm = getPtm();
    for (const Painted& p : _painted) {
        b2Vec2 c = p.body->GetWorldCenter();
        p.sprite->setPosition(Vec2(c.x * ptm, c.y * ptm));
        p.sprite->setRotation(-CC_RADIANS_TO_DEGREES(p.body->GetAngle()));
    }
    for (Clamp* c : {&_front, &_back}) {
        Sprite* s = c == &_front ? _frontWheelSprite : _backWheelSprite;
        b2Vec2 p = c->wheel->GetWorldCenter();
        s->setPosition(Vec2(p.x * ptm, p.y * ptm));
        s->setRotation(-CC_RADIANS_TO_DEGREES(c->wheel->GetAngle()));
    }
}

void MineCart::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    Vehicle::beginContact(fixture, otherFixture, contact);
}

void MineCart::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    Vehicle::endContact(fixture, otherFixture, contact);
}

// Frame hits above the smash limit; while clamping, a wheel touching a rail (material 4) marks it
// to be attached next frame (Flash wheelContactResult).
void MineCart::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                         const b2ContactImpulse* impulse)
{
    float normalImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        normalImpulse = b2Max(normalImpulse, impulse->normalImpulses[1]);
    }
    if (fixture == _frontWheelFixture || fixture == _backWheelFixture) {
        if (_connecting && (getLevel()->getFixtureMaterial(otherFixture) & 4)) {
            Clamp& c = fixture == _frontWheelFixture ? _front : _back;
            b2Body* rail = otherFixture->GetBody();
            if (!c.newRail && c.rail != rail) {
                c.newRail = rail;
            }
        }
        return;
    }
    if (fixture == _bottomFixture || fixture == _leftFixture || fixture == _rightFixture) {
        auto limit = _contactImpulseDict.find(fixture);
        if (limit != _contactImpulseDict.end() && normalImpulse > limit->second) {
            _results[fixture] = std::max(_results[fixture], normalImpulse);
        }
    }
}

void MineCart::handleContactResults()
{
    bool smash = !_results.empty();
    _results.clear();
    if (smash && !_frameSmashed) {
        frameSmash();
    }
}

void MineCart::debugFunction(int value)
{
    if (!_frameSmashed) {
        frameSmash();
    }
}

// Flash frameSmash: the explorer is thrown out and the cart breaks into the cart (bottom and both
// walls), the frame sides and bottom and the engine, with a burst of 50 cart shards.
void MineCart::frameSmash()
{
    _frameSmashed = true;
    _vehicleSmashed = true;
    if (_rider && !_riderEjected) {
        ejectCharacter(_rider);
    }
    removeDongle(_front);
    removeDongle(_back);
    _frontWheelFixture->SetFilterData(_zeroFilter);
    _backWheelFixture->SetFilterData(_zeroFilter);
    for (b2Fixture* f : {_bottomFixture, _leftFixture, _rightFixture}) {
        removePostSolve(f);
        fixtureWillBeDestroyed(f);
        _contactImpulseDict.erase(f);
    }

    b2World* world = getWorld();
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = _frameBody->GetPosition();
    def.angle = _frameBody->GetAngle();
    b2FixtureDef fixture;
    fixture.density = 1.5f;
    fixture.friction = 0.3f;
    fixture.restitution = 0.1f;
    fixture.filter.categoryBits = 0x202;
    float angularVelocity = _frameBody->GetAngularVelocity();

    struct PieceDef
    {
        std::vector<const char*> shapes;
        const char* sprite;
    };
    const PieceDef pieces[] = {
        {{"cartBottomShape", "cartLeftShape", "cartRightShape"}, "cartSmashed"},
        {{"frameLeftShape"}, "frameLeftSmashed"},
        {{"frameRightShape"}, "frameRightSmashed"},
        {{"frame2BottomShape"}, "frameBottomSmashed"},
        {{"engineShape"}, "engineSmashed"},
    };
    b2Body* cart = nullptr;
    b2Body* engine = nullptr;
    for (const PieceDef& piece : pieces) {
        b2Body* body = world->CreateBody(&def);
        for (const char* name : piece.shapes) {
            addBox(body, fixture, name);
        }
        body->ResetMassData();
        body->SetLinearVelocity(_frameBody->GetLinearVelocityFromLocalPoint(body->GetLocalCenter()));
        body->SetAngularVelocity(angularVelocity);
        paintBody(body, piece.sprite);
        cart = cart ? cart : body;
        engine = body;
    }
    if (gameplay()) {
        createBodySound("MetalSmashHeavy", cart, 1.0f, false);
    }
    // createBurst("cartshards", 30, 30, engine, 50)
    std::vector<std::string> frames;
    for (int i = 1; i <= 12; i++) {
        frames.push_back(_name + "_cartShard_" + patch::to_string(i) + ".png");
    }
    if (restored::FlashParticles* particles = restored::FlashParticles::forSession(getSession())) {
        particles->burst(frames, engine->GetWorldCenter(), engine->GetLinearVelocity(), 30.0f, 30.0f, 50);
    }

    _painted.erase(std::remove_if(_painted.begin(), _painted.end(),
                                  [this](const Painted& p) { return p.body == _frameBody; }),
                   _painted.end());
    _frameSprite->setVisible(false);
    stopSoundsForBody(_frameBody);
    // (the rider's joints went with the eject; the wheel joints go with the frame)
    world->DestroyBody(_frameBody);
    _frameBody = nullptr;
    _front.wheelJoint = nullptr;
    _back.wheelJoint = nullptr;
    _wheelJoints.clear();
    _wheelJointSpeedDict.clear();
    _bottomFixture = _leftFixture = _rightFixture = nullptr;
}

// ---- QOL (PC addition): re-grab vehicle (src/game/vehicles/Vehicle.h) ---------------------------

b2Body* MineCart::qolFrameBody()
{
    return _frameBody;
}

// checkStateOfCharacter (and handleInjury): off once both hands and both feet have let go.
bool MineCart::qolCanRemount(CharacterB2D* character)
{
    return character == _rider && !_frameSmashed && _frameBody &&
           !(character->qolLostLowerArm(1) && character->qolLostLowerArm(2) &&
             character->qolLostLowerLeg(1) && character->qolLostLowerLeg(2));
}

// ejectCharacter: _riderEjected / _ejected, not connecting, the cart in group -2 with the zero
// filter (qolRestoreFilters), the explorer's lower legs shown - the cart hides the ones still on.
void MineCart::qolRemount(CharacterB2D* character)
{
    _riderEjected = false;
    _ejected = false;
    qolRestoreFilters(character);
    qolMount(character, [this, character]() { addCharacter(character); });
    if (ExplorerGuy* explorer = dynamic_cast<ExplorerGuy*>(character)) {
        explorer->hideLowerLegs(!character->qolLostLowerLeg(1, false), !character->qolLostLowerLeg(2, false));
    }
    qolReplayInjuries(character);
}
