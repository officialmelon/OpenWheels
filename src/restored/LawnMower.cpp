// RESTORED (PC addition): Lawnmower Man's mower, ported from the browser game's LawnMowerMan.as
// (Flash v1.87) onto the mobile Vehicle framework. See LawnMower.h.

#include "LawnMower.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "BurstEmitter.h"
#include "CharacterB2D.h"
#include "EmitterNode.h"
#include "LevelB2D.h"
#include "Patch.h"
#include "Session.h"
#include "Settings.h"
#include "Sound.h"
#include "online/items/Grindable.h"  // ONLINE (PC addition): NPCs / food of browser levels
#include "online/FlashPhysics.h"  // RESTORED (PC addition): per-step constants at 1/30 too

USING_NS_CC;

namespace {

// Flash lengths: the world is 62.5 px per metre (Flash m_physScale), character art 125 symbol px
// per metre - the same metres as the mobile engine's. kPhys converts the Flash code's px / 30.
const float kPhys = 0.48f;

b2Vec2 toB2(const Vec2& v)
{
    return b2Vec2(v.x, v.y);
}

}  // namespace

LawnMower* LawnMower::create(Vec2 position, std::string name, int groupID)
{
    LawnMower* mower = new (std::nothrow) LawnMower();
    if (mower && mower->init(position, name, groupID)) {
        mower->autorelease();
        return mower;
    }
    delete mower;
    return nullptr;
}

LawnMower::LawnMower()
    : _wheelSpeedRatio(1.76f),
      _impulseLeft(2.4f),
      _impulseRight(2.8f),
      _impulseOffset(1.0f),
      _maxSpinAV(3.5f),
      _mowerSmashLimit(200.0f),
      _frontRearSmashLimit(150.0f),
      _ejectImpulse(5.0f),
      _verticalTranslation(20.0f / 30.0f * kPhys),
      _mowerBody(nullptr),
      _frontShockBody(nullptr),
      _backShockBody(nullptr),
      _frontWheelBody(nullptr),
      _backWheelBody(nullptr),
      _frontBody(nullptr),
      _rearBody(nullptr),
      _handleFixture(nullptr),
      _shaftFixture(nullptr),
      _frontFixture(nullptr),
      _baseFixture(nullptr),
      _bladeFixture(nullptr),
      _rearFixture(nullptr),
      _topFixture(nullptr),
      _seatFixture(nullptr),
      _clearanceFixture(nullptr),
      _brokenFrontFixture(nullptr),
      _brokenRearFixture(nullptr),
      _frontShockJoint(nullptr),
      _backShockJoint(nullptr),
      _frontWheelJoint(nullptr),
      _backWheelJoint(nullptr),
      _rider(nullptr),
      _mowerSprite(nullptr),
      _bladeCoverSprite(nullptr),
      _frontWheelSprite(nullptr),
      _backWheelSprite(nullptr),
      _shockNode(nullptr),
      _mowerLoop(nullptr),
      _grindLoop(nullptr),
      _soundDelay(10),
      _soundDelayCount(0),
      _impactSoundPlaying(false),
      _mowerSmashed(false),
      _frameCounter(0),
      _bladeHalfWidth(1.0f),
      _bladeBottom(0.0f),
      _mowerMass(1.0f),
      _frontBreakImpulse(0.0f),
      _rearBreakImpulse(0.0f)
{
    _frontWheelFixture = nullptr;
    _backWheelFixture = nullptr;
}

LawnMower::~LawnMower()
{
    if (_mowerLoop) {
        _mowerLoop->setFinishCallback(nullptr);
    }
    if (_grindLoop) {
        _grindLoop->setFinishCallback(nullptr);
    }
}

bool LawnMower::gameplay()
{
    return getSession()->getMode() == SessionModeGameplay;
}

bool LawnMower::init(Vec2 position, std::string name, int groupID)
{
    // Flash wheelMaxSpeed 15, accelStep 3, maxTorque 100000 (mobile: forward is negative and the
    // per-frame step halves at 60 Hz, as in the port's MotorCart).
    _maxSpeed = -15.0f;
    _accelStep = 1.5f;
    _maxTorque = 100000.0f;
    bool ok = Vehicle::init(position, name, groupID);
    if (ok) {
        getLevel()->addToPaintItem(this);
        _wheelSoundVolume = 0.3f;
        if (gameplay()) {
            startLoop(_mowerLoop, "MowerLoop", 0.5f, 1.0f);
        }
    }
    return ok;
}

ValueMap LawnMower::shape(const std::string& name)
{
    return _bodiesDict.at("bodies").asValueMap().at(name).asValueMap();
}

Vec2 LawnMower::point(const std::string& name)
{
    return PointFromString(_bodiesDict.at("joints").asValueMap().at(name).asString());
}

b2Fixture* LawnMower::addPolygon(b2Body* body, b2FixtureDef def, const std::string& prefix,
                                 int first, int count)
{
    b2Vec2 vertices[b2_maxPolygonVertices];
    for (int i = 0; i < count; i++) {
        vertices[i] = toB2(point(prefix + patch::to_string(first + i)));
    }
    b2PolygonShape polygon;
    polygon.Set(vertices, count);
    def.shape = &polygon;
    return body->CreateFixture(&def);
}

b2Fixture* LawnMower::addBox(b2Body* body, b2FixtureDef def, const std::string& name)
{
    ValueMap data = shape(name);
    return createFixture(body, def, &data, true, true);
}

void LawnMower::lockWheels()
{
    if (_backWheelJoint) {
        _backWheelJoint->EnableLimit(true);
        _backWheelJoint->SetLimits(0.0f, 0.0f);
    }
}

void LawnMower::createSprites()
{
    Node* vehicleBackground = getSession()->getVehicleBackground();
    _shockNode = DrawNode::create();
    vehicleBackground->addChild(_shockNode);
    _mowerSprite = Sprite::createWithSpriteFrameName(_name + "_mower.png");
    vehicleBackground->addChild(_mowerSprite);
    _backWheelSprite = Sprite::createWithSpriteFrameName(_name + "_backWheel.png");
    vehicleBackground->addChild(_backWheelSprite);
    _frontWheelSprite = Sprite::createWithSpriteFrameName(_name + "_frontWheel.png");
    vehicleBackground->addChild(_frontWheelSprite);
    // Flash puts the blade cover above the rider's front arm: it hides what the blade pulls in.
    _bladeCoverSprite = Sprite::createWithSpriteFrameName(_name + "_bladeCover.png");
    getSession()->getVehicleForeground()->addChild(_bladeCoverSprite, 1000);
}

void LawnMower::createBodies()
{
    b2World* world = getWorld();

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position.Set(_origin.x, _origin.y);

    b2FixtureDef def;
    def.userData = this;
    def.density = 4.0f;
    def.friction = 0.3f;
    def.restitution = 0.1f;
    def.filter = _zeroFilter;
    def.filter.groupIndex = -2;

    _mowerBody = world->CreateBody(&bodyDef);
    _handleFixture = addPolygon(_mowerBody, def, "handleVert", 1, 4);
    _shaftFixture = addBox(_mowerBody, def, "shaftShape");
    _frontFixture = addPolygon(_mowerBody, def, "frontVert", 1, 6);
    _baseFixture = addBox(_mowerBody, def, "baseShape");
    _bladeFixture = addBox(_mowerBody, def, "bladeShape");
    _rearFixture = addPolygon(_mowerBody, def, "rearVert", 1, 5);
    _topFixture = addPolygon(_mowerBody, def, "topVert", 1, 5);
    _seatFixture = addPolygon(_mowerBody, def, "backVert", 1, 4);

    // Alignment pads at both ends of the deck and the clearance sensor below the blade: the only
    // things a body being ground still touches (category 1, Flash filter).
    b2FixtureDef pad = def;
    pad.filter.categoryBits = 1;
    pad.filter.maskBits = 1;
    pad.filter.groupIndex = 0;
    pad.friction = 0.01f;
    pad.density = 0.01f;
    addBox(_mowerBody, pad, "alignShape1");
    addBox(_mowerBody, pad, "alignShape2");
    pad.isSensor = true;
    _clearanceFixture = addBox(_mowerBody, pad, "clearanceShape");
    {
        // Flash clearanceShape.m_vertices[0]: the sensor's top edge (y up here).
        const b2PolygonShape* box = static_cast<b2PolygonShape*>(_clearanceFixture->GetShape());
        _clearanceTop = box->m_vertices[0].y;
        for (int i = 1; i < box->m_count; i++) {
            _clearanceTop = std::max(_clearanceTop, box->m_vertices[i].y);
        }
    }
    _mowerBody->ResetMassData();
    _mowerMass = _mowerBody->GetMass();

    ValueMap blade = shape("bladeShape");
    Vec2 bladePos = PointFromString(blade.at("pos").asString());
    Size bladeSize = SizeFromString(blade.at("size").asString());
    _bladeCenter = toB2(bladePos);
    _bladeHalfWidth = bladeSize.width;
    _bladeBottom = bladePos.y - bladeSize.height;

    // Wheels (Flash: density 5, friction 1, restitution 0.3).
    b2FixtureDef wheel = def;
    wheel.density = 5.0f;
    wheel.friction = 1.0f;
    wheel.restitution = 0.3f;
    b2CircleShape circle;
    wheel.shape = &circle;
    ValueMap backWheel = shape("backWheelShape");
    ValueMap frontWheel = shape("frontWheelShape");
    Vec2 backPos = PointFromString(backWheel.at("pos").asString()) + _origin;
    Vec2 frontPos = PointFromString(frontWheel.at("pos").asString()) + _origin;
    circle.m_radius = backWheel.at("radius").asFloat();
    bodyDef.position.Set(backPos.x, backPos.y);
    _backWheelBody = world->CreateBody(&bodyDef);
    _backWheelFixture = _backWheelBody->CreateFixture(&wheel);
    circle.m_radius = frontWheel.at("radius").asFloat();
    bodyDef.position.Set(frontPos.x, frontPos.y);
    _frontWheelBody = world->CreateBody(&bodyDef);
    _frontWheelFixture = _frontWheelBody->CreateFixture(&wheel);

    // Shock bodies (12 / character_scale Flash m boxes) the wheels turn on.
    b2PolygonShape box;
    float half = 12.0f / 60.0f * kPhys;
    b2FixtureDef shock = def;
    box.SetAsBox(half, half);
    shock.shape = &box;
    bodyDef.position.Set(frontPos.x, frontPos.y);
    _frontShockBody = world->CreateBody(&bodyDef);
    _frontShockBody->CreateFixture(&shock);
    bodyDef.position.Set(backPos.x, backPos.y);
    _backShockBody = world->CreateBody(&bodyDef);
    _backShockBody->CreateFixture(&shock);

    for (b2Body* body : {_mowerBody, _frontWheelBody, _backWheelBody, _frontShockBody, _backShockBody}) {
        body->ResetMassData();
    }

    addToPostSolve(_frontFixture);
    addToPostSolve(_rearFixture);
    addToPostSolve(_topFixture);
    addToPostSolve(_seatFixture);
    addToPostSolve(_bladeFixture);
    addToBeginContact(_frontFixture);
    addToBeginContact(_topFixture);
    addToBeginContact(_seatFixture);
    addToBeginContact(_clearanceFixture);
    addToEndContact(_clearanceFixture);

    _mowerBody->SetUserData(_mowerSprite);
    _frontWheelBody->SetUserData(_frontWheelSprite);
    _backWheelBody->SetUserData(_backWheelSprite);
    getLevel()->addToPaintBody(_mowerBody);
    getLevel()->addToPaintBody(_frontWheelBody);
    getLevel()->addToPaintBody(_backWheelBody);
}

void LawnMower::createJoints()
{
    b2World* world = getWorld();
    b2PrismaticJointDef shockDef;
    shockDef.maxMotorForce = 1000.0f;
    shockDef.enableLimit = true;
    shockDef.lowerTranslation = 0.0f;
    shockDef.upperTranslation = 0.0f;
    shockDef.Initialize(_mowerBody, _frontShockBody, _frontWheelBody->GetPosition(), b2Vec2(0.0f, 1.0f));
    _frontShockJoint = static_cast<b2PrismaticJoint*>(world->CreateJoint(&shockDef));
    shockDef.Initialize(_mowerBody, _backShockBody, _backWheelBody->GetPosition(), b2Vec2(0.0f, 1.0f));
    _backShockJoint = static_cast<b2PrismaticJoint*>(world->CreateJoint(&shockDef));

    b2RevoluteJointDef wheelDef;
    wheelDef.maxMotorTorque = _maxTorque;
    wheelDef.Initialize(_backShockBody, _backWheelBody, _backWheelBody->GetPosition());
    _backWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&wheelDef));
    wheelDef.Initialize(_frontShockBody, _frontWheelBody, _frontWheelBody->GetPosition());
    _frontWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&wheelDef));

    addWheelJoint(_backWheelJoint, _backWheelBody);
    addWheelJoint(_frontWheelJoint, _frontWheelBody);
    // Flash: front motor = 1.76 x back (the smaller front wheel), not the radius ratio.
    _wheelJointSpeedDict[_frontWheelJoint] = _wheelSpeedRatio;
}

void LawnMower::createDictionaries()
{
    _contactImpulseDict[_frontFixture] = _mowerSmashLimit;
    _contactAddSounds[_backWheelFixture] = "CarTire1";
    _contactAddSounds[_frontWheelFixture] = "CarTire1";
    _contactAddSounds[_frontFixture] = "ChairHit3";
    _contactAddSounds[_topFixture] = "ChairHit2";
    _contactAddSounds[_seatFixture] = "BikeHit3";
}

// Flash LawnMowerMan.createJoints: pelvis on the seat, hands on the handle, feet on the footrest;
// the legs are sensors while riding (createBodies).
void LawnMower::addCharacter(CharacterB2D* character)
{
    Vehicle::addCharacter(character);
    _rider = character;
    b2World* world = getWorld();

    float neckAngle = character->getHeadBody()->GetAngle() - character->getChestBody()->GetAngle();
    character->getNeckJoint()->SetLimits(-0.17453292f - neckAngle, 0.17453292f - neckAngle);

    for (b2Body* leg : {character->getUpperLeg1Body(), character->getUpperLeg2Body(),
                        character->getLowerLeg1Body(), character->getLowerLeg2Body()}) {
        leg->GetFixtureList()->SetSensor(true);
    }

    b2RevoluteJointDef def;
    def.maxMotorTorque = _maxTorque;
    b2Vec2 pelvis = character->getPelvisBody()->GetWorldCenter();
    def.Initialize(_mowerBody, character->getPelvisBody(), pelvis);
    addBodyVehicleJoint(character->getPelvisBody(), world->CreateJoint(&def));

    b2Vec2 handle = toB2(point("handleAnchor") + _origin);
    def.Initialize(_mowerBody, character->getLowerArm1Body(), handle);
    addBodyVehicleJoint(character->getLowerArm1Body(), world->CreateJoint(&def));
    def.Initialize(_mowerBody, character->getLowerArm2Body(), handle);
    addBodyVehicleJoint(character->getLowerArm2Body(), world->CreateJoint(&def));

    b2Vec2 foot = toB2(point("footAnchor") + _origin);
    def.Initialize(_mowerBody, character->getLowerLeg1Body(), foot);
    addBodyVehicleJoint(character->getLowerLeg1Body(), world->CreateJoint(&def));
    def.Initialize(_mowerBody, character->getLowerLeg2Body(), foot);
    addBodyVehicleJoint(character->getLowerLeg2Body(), world->CreateJoint(&def));
}

void LawnMower::handleInjury(CharacterInjury injury, CharacterB2D* character)
{
    // Flash LawnMowerMan ejects on every head smash too (headSmash1 -> eject).
    if (injury == CharacterInjuryHeadSmash) {
        handleFatalWound(character);
        return;
    }
    Vehicle::handleInjury(injury, character);
}

// Flash checkEject: off once both hands and both feet have let go.
void LawnMower::checkStateOfCharacter(CharacterB2D* character)
{
    if (_bodyVehicleJointDict[character->getLowerArm1Body()] == nullptr &&
        _bodyVehicleJointDict[character->getLowerArm2Body()] == nullptr &&
        _bodyVehicleJointDict[character->getLowerLeg1Body()] == nullptr &&
        _bodyVehicleJointDict[character->getLowerLeg2Body()] == nullptr) {
        ejectCharacter(character);
    }
}

bool LawnMower::ejectCharacter(CharacterB2D* character)
{
    if (_ejected) {
        return false;
    }
    _ejected = true;
    forwardBackButtonsNull();
    leanButtonsNull();
    Vehicle::ejectCharacter(character);

    for (b2Body* leg : {character->getUpperLeg1Body(), character->getUpperLeg2Body(),
                        character->getLowerLeg1Body(), character->getLowerLeg2Body()}) {
        if (leg && leg->GetFixtureList()) {
            leg->GetFixtureList()->SetSensor(false);
        }
    }
    if (!_mowerSmashed) {
        _frontShockJoint->EnableMotor(false);
        _frontShockJoint->SetLimits(0.0f, 0.0f);
        _frontShockJoint->SetMotorSpeed(0.0f);
        _backShockJoint->EnableMotor(false);
        _backShockJoint->SetLimits(0.0f, 0.0f);
        _backShockJoint->SetMotorSpeed(0.0f);
    }
    // Pushed off along the mower's up direction.
    b2Body* mower = _mowerBody ? _mowerBody : _rearBody;
    if (mower) {
        float angle = mower->GetAngle() + (float)M_PI_2;
        b2Vec2 impulse(cosf(angle) * _ejectImpulse, sinf(angle) * _ejectImpulse);
        b2Body* chest = character->getChestBody();
        b2Body* pelvis = character->getPelvisBody();
        if (chest) {
            chest->ApplyLinearImpulse(impulse, chest->GetWorldCenter(), true);
        }
        if (pelvis) {
            pelvis->ApplyLinearImpulse(impulse, pelvis->GetWorldCenter(), true);
        }
    }
    return true;
}

// ---- controls -------------------------------------------------------------------------------

void LawnMower::forwardButtonPressed()
{
    if (_mowerSmashed) {
        return;
    }
    // RESTORED (PC addition): 1.5 per 60 Hz step (Flash 3 per frame) at the current step.
    _accelStep = online::perStep(1.5f);
    Vehicle::forwardButtonPressed();
    fadeLoop(_mowerLoop, 1.0f, 0.25f, false);
}

void LawnMower::backButtonPressed()
{
    if (_mowerSmashed) {
        return;
    }
    _accelStep = online::perStep(1.5f);  // RESTORED (PC addition), as above
    Vehicle::backButtonPressed();
    fadeLoop(_mowerLoop, 1.0f, 0.25f, false);
}

void LawnMower::forwardBackButtonsNull()
{
    bool wasDriving = !_wheelJoints.empty() && _wheelJoints[0]->IsMotorEnabled();
    Vehicle::forwardBackButtonsNull();
    if (wasDriving) {
        fadeLoop(_mowerLoop, 0.5f, 0.25f, false);
    }
}

// Flash leftPressedActions (impulseLeft): nose up.
void LawnMower::leanBackButtonPressed()
{
    if (_mowerSmashed) {
        return;
    }
    setCurrentPose(VehiclePoseLeanBack);
    float angularVelocity = _mowerBody->GetAngularVelocity();
    float angle = _mowerBody->GetAngle();
    float spin = std::min(std::max((angularVelocity - _maxSpinAV) / -_maxSpinAV, 0.0f), 1.0f);
    float magnitude = _impulseLeft * s_timeStepOverFlashTimeStep * spin;
    b2Vec2 localCenter = _mowerBody->GetLocalCenter();
    b2Vec2 down(sinf(angle) * magnitude, -cosf(angle) * magnitude);
    _mowerBody->ApplyLinearImpulse(
        down, _mowerBody->GetWorldPoint(b2Vec2(localCenter.x - _impulseOffset, localCenter.y)), true);
    _mowerBody->ApplyLinearImpulse(
        -down, _mowerBody->GetWorldPoint(b2Vec2(localCenter.x + _impulseOffset, localCenter.y)), true);
}

// Flash rightPressedActions (impulseRight): nose down.
void LawnMower::leanForwardButtonPressed()
{
    if (_mowerSmashed) {
        return;
    }
    setCurrentPose(VehiclePoseLeanForward);
    float angularVelocity = _mowerBody->GetAngularVelocity();
    float angle = _mowerBody->GetAngle();
    float spin = std::min(std::max((angularVelocity + _maxSpinAV) / _maxSpinAV, 0.0f), 1.0f);
    float magnitude = _impulseRight * s_timeStepOverFlashTimeStep * spin;
    b2Vec2 localCenter = _mowerBody->GetLocalCenter();
    b2Vec2 down(sinf(angle) * magnitude, -cosf(angle) * magnitude);
    _mowerBody->ApplyLinearImpulse(
        down, _mowerBody->GetWorldPoint(b2Vec2(localCenter.x + _impulseOffset, localCenter.y)), true);
    _mowerBody->ApplyLinearImpulse(
        -down, _mowerBody->GetWorldPoint(b2Vec2(localCenter.x - _impulseOffset, localCenter.y)), true);
}

// Flash leanBackPose / leanForwardPose (setJoint(j, a, g) -> mobile setJoint(j, -a, g, 20)).
void LawnMower::leanBackPose()
{
    if (!_rider || _ejected) {
        return;
    }
    _rider->setJoint(_rider->getNeckJoint(), 0.0f, 2.0f, 20.0f);
    if (_rider->getElbowJoint1() && _rider->getShoulderJoint1() && !_rider->getUpperArm3Body()) {
        _rider->setJoint(_rider->getElbowJoint1(), -2.5f, 15.0f, 20.0f);
    }
    if (_rider->getElbowJoint2() && _rider->getShoulderJoint2() && !_rider->getUpperArm4Body()) {
        _rider->setJoint(_rider->getElbowJoint2(), -2.5f, 15.0f, 20.0f);
    }
}

void LawnMower::leanForwardPose()
{
    if (!_rider || _ejected) {
        return;
    }
    _rider->setJoint(_rider->getNeckJoint(), -1.0f, 1.0f, 20.0f);
    if (_rider->getElbowJoint1() && _rider->getShoulderJoint1() && !_rider->getUpperArm3Body()) {
        _rider->setJoint(_rider->getElbowJoint1(), 0.0f, 15.0f, 20.0f);
    }
    if (_rider->getElbowJoint2() && _rider->getShoulderJoint2() && !_rider->getUpperArm4Body()) {
        _rider->setJoint(_rider->getElbowJoint2(), 0.0f, 15.0f, 20.0f);
    }
}

// Flash spacePressedActions: raise the deck (shocks pushed down) and hold it up. The Flash shock
// axis points down (y down); here it points up, so translations and speeds change sign.
void LawnMower::special1ButtonPressed()
{
    if (_mowerSmashed || _ejected) {
        return;
    }
    float vt = _verticalTranslation;
    if (!_backShockJoint->IsMotorEnabled()) {
        if (_backShockJoint->GetLowerLimit() != -vt) {
            for (b2PrismaticJoint* j : {_frontShockJoint, _backShockJoint}) {
                j->SetMotorSpeed(-2.5f);
                j->SetLimits(-vt, 0.0f);
                j->EnableMotor(true);
            }
            if (gameplay()) {
                createBodySound("SegwayJump", _backWheelBody, 1.0f, false);
            }
        }
    } else if (_backShockJoint->GetMotorSpeed() < 0.0f) {
        if (_backShockJoint->GetJointTranslation() < -vt) {
            for (b2PrismaticJoint* j : {_frontShockJoint, _backShockJoint}) {
                j->EnableMotor(false);
                j->SetLimits(-vt, -vt + 0.01f);
                j->SetMotorSpeed(0.0f);
            }
        }
    } else if (_backShockJoint->GetMotorSpeed() > 0.0f) {
        if (_backShockJoint->GetJointTranslation() > 0.0f) {
            for (b2PrismaticJoint* j : {_frontShockJoint, _backShockJoint}) {
                j->EnableMotor(false);
                j->SetLimits(0.0f, 0.0f);
                j->SetMotorSpeed(0.0f);
            }
        }
    }
}

// Flash spaceNullActions: let the deck back down.
void LawnMower::special1ButtonNull()
{
    if (_mowerSmashed || _ejected) {
        return;
    }
    float vt = _verticalTranslation;
    if (_backShockJoint->IsMotorEnabled()) {
        if (_backShockJoint->GetMotorSpeed() < 0.0f) {
            if (_frontShockJoint->GetJointTranslation() < -vt) {
                _frontShockJoint->SetMotorSpeed(1.0f);
                _backShockJoint->SetMotorSpeed(1.0f);
            }
        } else if (_backShockJoint->GetMotorSpeed() > 0.0f) {
            if (_backShockJoint->GetJointTranslation() > 0.0f) {
                for (b2PrismaticJoint* j : {_frontShockJoint, _backShockJoint}) {
                    j->EnableMotor(false);
                    j->SetLimits(0.0f, 0.0f);
                    j->SetMotorSpeed(0.0f);
                }
            }
        }
    } else if (_backShockJoint->GetLowerLimit() != 0.0f) {
        for (b2PrismaticJoint* j : {_frontShockJoint, _backShockJoint}) {
            j->SetMotorSpeed(1.0f);
            j->SetLimits(-vt, 0.0f);
            j->EnableMotor(true);
        }
    }
}

// ---- per frame ------------------------------------------------------------------------------

void LawnMower::actions()
{
    Vehicle::actions();  // poses, contact sounds, handleContactResults (smashes, blade)
    _frameCounter++;

    // Finished targets: out of the clearance sensor (Flash actions()).
    for (int i = (int)_targets.size() - 1; i >= 0; i--) {
        Target target = _targets[i];
        if (_contactCount[target.body] == 0) {
            _targets.erase(_targets.begin() + i);
            finishTarget(target);
            if (_targets.empty() && _addedTargets.empty()) {
                fadeLoop(_grindLoop, 0.0f, 0.3f, true);
            }
        }
    }
    for (const Target& target : _addedTargets) {
        _targets.push_back(target);
    }
    _addedTargets.clear();

    // Blood spraying out under the deck while something is in it (Flash createBloodSpray).
    if (_mowerBody && !_targets.empty() && (_frameCounter % 3) == 0) {
        EmitterNode* particles = getSession()->getParticlesForeground();
        for (const Target& target : _targets) {
            // ONLINE (PC addition): food sprays its own juice in Flash, not blood.
            if (!target.owner) {
                online::Grindable* g = online::Grindable::forBody(target.body);
                if (g && g->grindSprayType() != 0) continue;
            }
            int count = (int)roundf(8.0f * target.massRatio) + 2;
            float x = target.leftX + (target.rightX - target.leftX) * CCRANDOM_0_1();
            BurstEmitter* burst = BurstEmitter::createBloodBurst(
                3.0f, 10.0f, _mowerBody, b2Vec2(x, _bladeBottom), count);
            if (burst && particles) {
                particles->addChild(burst);
            }
        }
    }

    if (_impactSoundPlaying) {
        _soundDelayCount++;
        if (_soundDelayCount >= _soundDelay) {
            _impactSoundPlaying = false;
            _soundDelayCount = 0;
            _soundDelay = (int)roundf(CCRANDOM_0_1() * 20.0f) + 5;
        }
    }
}

void LawnMower::paint()
{
    float ptm = getPtm();
    if (_bladeCoverSprite && _mowerBody) {
        b2Vec2 p = _mowerBody->GetPosition();
        _bladeCoverSprite->setPosition(Vec2(p.x * ptm, p.y * ptm));
        _bladeCoverSprite->setRotation(-CC_RADIANS_TO_DEGREES(_mowerBody->GetAngle()));
    }
    if (_shockNode) {
        _shockNode->clear();
        if (!_mowerSmashed) {
            Color4F colour(Color3B(0x17, 0x17, 0x17));
            for (b2PrismaticJoint* j : {_frontShockJoint, _backShockJoint}) {
                b2Vec2 a = j->GetAnchorA();
                b2Vec2 b = j->GetBodyB()->GetWorldCenter();
                _shockNode->drawSegment(Vec2(a.x * ptm, a.y * ptm), Vec2(b.x * ptm, b.y * ptm),
                                        ptm * 1.5f / 62.5f, colour);
            }
        }
    }
    for (const Piece& piece : _pieces) {
        b2Vec2 c = piece.body->GetWorldCenter();
        piece.sprite->setPosition(Vec2(c.x * ptm, c.y * ptm));
        piece.sprite->setRotation(-CC_RADIANS_TO_DEGREES(piece.body->GetAngle()));
    }
    // What the blade pulls up past the clearance sensor disappears into the deck: Flash masks
    // the art (maskTarget); art that can't be clipped (inside a batch node) is hidden once its
    // body passes the blade's lower edge.
    if (_mowerBody) {
        for (const Target& target : _targets) {
            if (target.clip) {
                updateMask(target);
                continue;
            }
            Node* node = static_cast<Node*>(target.body->GetUserData());
            if (node && _mowerBody->GetLocalPoint(target.body->GetWorldCenter()).y > _bladeBottom) {
                node->setVisible(false);
            }
        }
    }
    for (b2Body* body : _groundBodies) {
        Node* node = static_cast<Node*>(body->GetUserData());
        if (node) {
            node->setVisible(false);
        }
    }
}

// ---- contacts -------------------------------------------------------------------------------

void LawnMower::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (fixture == _clearanceFixture) {
        b2Body* body = otherFixture->GetBody();
        auto it = _contactCount.find(body);
        if (it != _contactCount.end()) {
            it->second++;
        }
        return;
    }
    Vehicle::beginContact(fixture, otherFixture, contact);
}

void LawnMower::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (fixture == _clearanceFixture) {
        b2Body* body = otherFixture->GetBody();
        auto it = _contactCount.find(body);
        if (it != _contactCount.end() && it->second > 0) {
            it->second--;
        }
        return;
    }
    Vehicle::endContact(fixture, otherFixture, contact);
}

void LawnMower::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                          const b2ContactImpulse* impulse)
{
    float normalImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        normalImpulse = b2Max(normalImpulse, impulse->normalImpulses[1]);
    }
    if (fixture == _bladeFixture && !_mowerSmashed) {
        // Flash contactBladeResultHandler: strongest contact per other shape.
        if (otherFixture->GetBody()->GetType() != b2_dynamicBody || otherFixture->IsSensor()) {
            return;
        }
        b2WorldManifold manifold;
        contact->GetWorldManifold(&manifold);
        for (BladeContact& c : _bladeContacts) {
            if (c.fixture == otherFixture) {
                if (normalImpulse > c.impulse) {
                    c.impulse = normalImpulse;
                    c.point = manifold.points[0];
                }
                return;
            }
        }
        _bladeContacts.push_back({otherFixture, normalImpulse, manifold.points[0]});
        return;
    }
    if (!_mowerSmashed &&
        (fixture == _frontFixture || fixture == _rearFixture || fixture == _topFixture ||
         fixture == _seatFixture)) {
        // Flash contactMowerResultHandler: every hard hit counts against the front's limit.
        if (normalImpulse > _mowerSmashLimit &&
            normalImpulse > _contactResultBufferDict[_frontFixture].impulse) {
            _contactResultBufferDict[_frontFixture].impulse = normalImpulse;
        }
        return;
    }
    if (fixture == _brokenFrontFixture || fixture == _handleFixture) {
        if (normalImpulse > _frontRearSmashLimit) {
            _frontBreakImpulse = std::max(_frontBreakImpulse, normalImpulse);
        }
        return;
    }
    for (b2Fixture* f : _rearContactFixtures) {
        if (f == fixture) {
            if (normalImpulse > _frontRearSmashLimit) {
                _rearBreakImpulse = std::max(_rearBreakImpulse, normalImpulse);
            }
            return;
        }
    }
}

void LawnMower::handleContactResults()
{
    if (!_mowerSmashed && _contactResultBufferDict[_frontFixture].impulse > 0.0f) {
        _contactResultBufferDict.clear();
        mowerSmash();
        return;
    }
    _contactResultBufferDict.clear();
    if (_frontBody && _frontBreakImpulse > 0.0f) {
        frontSmash();
    }
    if (_rearBody && _rearBreakImpulse > 0.0f) {
        rearSmash();
    }
    _frontBreakImpulse = 0.0f;
    _rearBreakImpulse = 0.0f;
    handleBladeContacts();
}

CharacterB2D* LawnMower::ownerOf(b2Body* body)
{
    std::vector<CharacterB2D*> characters = getLevel()->getCharacters();
    for (CharacterB2D* c : _characters) {
        characters.push_back(c);
    }
    if (_rider) {
        characters.push_back(_rider);
    }
    for (CharacterB2D* c : characters) {
        if (c && c->ownsBody(body)) {
            return c;
        }
    }
    return nullptr;
}

// Flash handleBladeContacts.
void LawnMower::handleBladeContacts()
{
    std::vector<BladeContact> contacts;
    contacts.swap(_bladeContacts);
    if (_mowerSmashed || !_mowerBody) {
        return;
    }
    b2World* world = getWorld();
    LevelB2D* level = getLevel();
    for (const BladeContact& c : contacts) {
        b2Fixture* other = c.fixture;
        b2Body* body = other->GetBody();
        if (!body->IsActive() || _contactCount.count(body)) {
            continue;
        }
        float mass = body->GetMass();
        b2Vec2 local = _mowerBody->GetLocalPoint(c.point) - _bladeCenter;
        float ratio = -local.x / _bladeHalfWidth;
        int material = level->getFixtureMaterial(other);
        if ((material & 7) && fabsf(ratio) < 0.5f) {
            // Caught: only the terrain, the deck pads and the clearance sensor still touch it.
            b2Filter filter;
            filter.categoryBits = 1;
            filter.maskBits = 9;
            filter.groupIndex = -3;
            for (b2Fixture* f = body->GetFixtureList(); f; f = f->GetNext()) {
                f->SetFilterData(filter);
            }
            level->removeFixtureMaterial(other);
            CharacterB2D* owner = ownerOf(body);
            if (owner) {
                owner->grindFixture(other);
            } else if (online::Grindable* g = online::Grindable::forBody(body)) {
                g->grindFixture(other);  // ONLINE (PC addition): browser NPC / food item
            }
            _contactCount[body] = 0;

            // Flash: mass ratio against 0.4 kg (the same kilograms as here).
            float massRatio = std::min(1.0f, mass / 0.4f);
            float halfSpray = 1.12f * massRatio * 0.5f;
            float x = _mowerBody->GetLocalPoint(c.point).x;
            Target target;
            target.body = body;
            target.owner = owner;
            target.massRatio = massRatio;
            target.leftX = std::max(_bladeCenter.x - _bladeHalfWidth, x - halfSpray);
            target.rightX = std::min(_bladeCenter.x + _bladeHalfWidth, x + halfSpray);

            b2BodyDef riserDef;
            riserDef.type = b2_dynamicBody;
            riserDef.position = c.point;
            riserDef.angle = _mowerBody->GetAngle();
            target.riser = world->CreateBody(&riserDef);
            b2PolygonShape box;
            box.SetAsBox(10.0f / 30.0f * kPhys, 10.0f / 30.0f * kPhys);
            b2FixtureDef riserFixture;
            riserFixture.shape = &box;
            riserFixture.density = 5.0f;
            riserFixture.friction = 0.1f;
            riserFixture.restitution = 0.1f;
            riserFixture.isSensor = true;
            target.riser->CreateFixture(&riserFixture);
            target.riser->ResetMassData();

            // Pulled up into the deck along the mower's axis (Flash: 0.5 Flash m/s).
            b2PrismaticJointDef slide;
            slide.enableLimit = true;
            slide.upperTranslation = 0.0f;
            slide.lowerTranslation = -10.0f;
            slide.maxMotorForce = 100000.0f;
            slide.enableMotor = true;
            slide.motorSpeed = -0.5f;
            slide.Initialize(_mowerBody, target.riser, c.point, _mowerBody->GetWorldVector(b2Vec2(0.0f, -1.0f)));
            world->CreateJoint(&slide);
            b2RevoluteJointDef hold;
            hold.enableMotor = true;
            hold.motorSpeed = 0.0f;
            hold.maxMotorTorque = 5.0f;
            hold.Initialize(target.riser, body, c.point);
            world->CreateJoint(&hold);
            if (mass > 0.1f) {
                maskTarget(target);  // Flash: only bodies heavier than 0.1 get the mask
            }
            _addedTargets.push_back(target);

            if (!_grindLoop && gameplay()) {
                startLoop(_grindLoop, "GrindLoop2", 1.0f, 0.2f);
            }
        } else {
            // Hit but not caught: blade and object knock each other apart (Flash: 3 per frame).
            float angle = -ratio * 1.2217f + (float)M_PI_2 + _mowerBody->GetAngle();
            float magnitude = 3.0f * s_timeStepOverFlashTimeStep;
            b2Vec2 dir(cosf(angle), sinf(angle));
            b2Vec2 spark = _mowerBody->GetWorldPoint(b2Vec2(_mowerBody->GetLocalPoint(c.point).x, _bladeBottom));
            if (mass > 0.0f) {
                float share = mass / (mass + _mowerMass);
                _mowerBody->ApplyLinearImpulse(magnitude * share * dir, c.point, true);
                body->ApplyLinearImpulse(-magnitude * (1.0f - share) * dir, c.point, true);
            } else {
                _mowerBody->ApplyLinearImpulse(magnitude * dir, c.point, true);
            }
            if (!_impactSoundPlaying && _targets.empty() && gameplay()) {
                _impactSoundPlaying = true;
                int n = 1 + (int)(CCRANDOM_0_1() * 2.999f);
                createPositionSound("MowerImpact" + patch::to_string(n), Vec2(c.point.x, c.point.y),
                                    1.0f, false);
                shardBurst(spark, 5, Color4B(255, 210, 90, 255));
            }
        }
    }
}

void LawnMower::maskTarget(Target& target)
{
    Node* node = static_cast<Node*>(target.body->GetUserData());
    Node* parent = node ? node->getParent() : nullptr;
    if (!parent || dynamic_cast<SpriteBatchNode*>(parent)) {
        return;
    }
    DrawNode* stencil = DrawNode::create();
    ClippingNode* clip = ClippingNode::create(stencil);
    const int z = node->getLocalZOrder();
    node->retain();
    node->removeFromParentAndCleanup(false);
    clip->addChild(node, z);
    parent->addChild(clip, z);
    node->release();
    target.clip = clip;
    target.stencil = stencil;
    target.artParent = parent;
    updateMask(target);
}

void LawnMower::updateMask(const Target& target)
{
    // Flash: drawRect(-100, top, 200, 100) px in the mower's frame, around its centre of mass,
    // from the top edge of the clearance sensor downwards (100 px = 100 / 30 Flash m).
    const float ptm = getPtm();
    const float reach = 100.0f / 30.0f * kPhys;
    const b2Vec2 c = _mowerBody->GetLocalCenter();
    const b2Vec2 local[4] = {b2Vec2(c.x - reach, _clearanceTop), b2Vec2(c.x + reach, _clearanceTop),
                             b2Vec2(c.x + reach, _clearanceTop - reach),
                             b2Vec2(c.x - reach, _clearanceTop - reach)};
    Vec2 points[4];
    for (int i = 0; i < 4; i++) {
        b2Vec2 w = _mowerBody->GetWorldPoint(local[i]);
        points[i] = Vec2(w.x * ptm, w.y * ptm);
    }
    target.stencil->clear();
    target.stencil->drawSolidPoly(points, 4, Color4F::WHITE);
}

void LawnMower::unmaskTarget(const Target& target)
{
    if (!target.clip) {
        return;
    }
    Node* node = static_cast<Node*>(target.body->GetUserData());
    if (node && node->getParent() == target.clip) {
        node->retain();
        node->removeFromParentAndCleanup(false);
        target.artParent->addChild(node, target.clip->getLocalZOrder());
        node->release();
    }
    target.clip->removeFromParent();
}

void LawnMower::finishTarget(const Target& target)
{
    unmaskTarget(target);
    // Flash: removeBody on the owner (its joints break), then the body is destroyed. Bodies are
    // kept (inactive, hidden) here: the character still holds pointers to its parts.
    getWorld()->DestroyBody(target.riser);
    _contactCount.erase(target.body);
    if (target.owner) {
        target.owner->grindBody(target.body);
    } else if (online::Grindable* g = online::Grindable::forBody(target.body)) {
        g->grindBody(target.body);  // ONLINE (PC addition): browser NPC / food item
    }
    target.body->SetActive(false);
    Node* node = static_cast<Node*>(target.body->GetUserData());
    if (node) {
        node->setVisible(false);
    }
    _groundBodies.push_back(target.body);
}

// ---- smashing -------------------------------------------------------------------------------

void LawnMower::shardBurst(b2Vec2 worldPoint, int count, Color4B color)
{
    Session* session = getSession();
    if (!session->canAddEmitter(count)) {
        return;
    }
    BurstEmitter* emitter = new (std::nothrow) BurstEmitter();
    emitter->setTotalParticles(count);
    Sprite* sprite = Sprite::create("images/cartShard.png");
    Rect rect = sprite->getTextureRect();
    __Array* textures = __Array::createWithCapacity(1);
    float ptm = session->getPtmRatio();
    if (emitter->init(textures, ptm, session->getTimeStep(), session->_gravity, rect.size.width * 0.6f,
                      rect.size.width, 0.0f, 12.566371f, 20.0f, 30.0f, nullptr, b2Vec2_zero,
                      Vec2(worldPoint.x * ptm, worldPoint.y * ptm), count)) {
        emitter->setTexture(sprite->getTexture());
        emitter->setStartColor(Color4F(color));
        emitter->autorelease();
        EmitterNode* particles = session->getParticlesForeground();
        if (particles) {
            particles->addChild(emitter);
        }
    } else {
        delete emitter;
    }
}

b2Body* LawnMower::makePiece(b2Body* from, const std::vector<std::vector<b2Vec2>>& polygons,
                             const std::string& sprite)
{
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = from->GetPosition();
    def.angle = from->GetAngle();
    b2Body* body = getWorld()->CreateBody(&def);
    b2FixtureDef fixture;
    fixture.density = 4.0f;
    fixture.friction = 0.3f;
    fixture.restitution = 0.1f;
    fixture.filter = _zeroFilter;
    for (const std::vector<b2Vec2>& vertices : polygons) {
        b2PolygonShape polygon;
        polygon.Set(vertices.data(), (int)vertices.size());
        fixture.shape = &polygon;
        body->CreateFixture(&fixture);
    }
    body->ResetMassData();
    body->SetAngularVelocity(from->GetAngularVelocity());
    body->SetLinearVelocity(from->GetLinearVelocityFromLocalPoint(body->GetLocalCenter()));
    Sprite* node = Sprite::createWithSpriteFrameName(_name + "_" + sprite + ".png");
    getSession()->getVehicleBackground()->addChild(node);
    _pieces.push_back({body, node});
    return body;
}

static std::vector<b2Vec2> verticesOf(b2Fixture* fixture)
{
    b2PolygonShape* polygon = static_cast<b2PolygonShape*>(fixture->GetShape());
    return std::vector<b2Vec2>(polygon->m_vertices, polygon->m_vertices + polygon->m_count);
}

// Flash mowerSmash: the rider is thrown off, whatever is in the deck is finished, and the mower
// splits into a front (hood, handle) and a rear (seat, deck) that keep their wheels.
void LawnMower::mowerSmash()
{
    _mowerSmashed = true;
    b2World* world = getWorld();
    for (const Target& t : _targets) {
        finishTarget(t);
    }
    for (const Target& t : _addedTargets) {
        finishTarget(t);
    }
    _targets.clear();
    _addedTargets.clear();
    fadeLoop(_grindLoop, 0.0f, 0.05f, true);
    fadeLoop(_mowerLoop, 0.0f, 0.05f, true);

    ejectAllCharacters();
    for (b2RevoluteJoint* joint : _wheelJoints) {
        joint->EnableMotor(false);
    }
    _wheelJoints.clear();
    _wheelJointSpeedDict.clear();

    for (b2Fixture* f : {_frontFixture, _rearFixture, _topFixture, _seatFixture, _bladeFixture}) {
        removePostSolve(f);
    }
    for (b2Fixture* f : {_frontFixture, _topFixture, _seatFixture, _clearanceFixture}) {
        removeBeginContact(f);
    }
    removeEndContact(_clearanceFixture);

    std::vector<b2Vec2> handle = verticesOf(_handleFixture);
    std::vector<b2Vec2> shaft = verticesOf(_shaftFixture);
    std::vector<b2Vec2> front = verticesOf(_frontFixture);
    std::vector<b2Vec2> base = verticesOf(_baseFixture);
    std::vector<b2Vec2> blade = verticesOf(_bladeFixture);
    std::vector<b2Vec2> rear = verticesOf(_rearFixture);
    std::vector<b2Vec2> top = verticesOf(_topFixture);
    std::vector<b2Vec2> seat = verticesOf(_seatFixture);
    std::vector<b2Vec2> seat2;
    for (int i = 1; i <= 4; i++) {
        seat2.push_back(toB2(point("seatVert" + patch::to_string(i))));
    }

    _frontBody = makePiece(_mowerBody, {handle, shaft, front}, "mowerFront");
    _rearBody = makePiece(_mowerBody, {base, blade, rear, top, seat, seat2}, "mowerRear");
    // Box2D lists fixtures newest first: the hood, the shaft, then the handle.
    _brokenFrontFixture = _handleFixture = nullptr;
    for (b2Fixture* f = _frontBody->GetFixtureList(); f; f = f->GetNext()) {
        f->SetUserData(this);
        addToPostSolve(f);
        if (!_brokenFrontFixture) {
            _brokenFrontFixture = f;
        }
        _handleFixture = f;
    }
    _contactAddSounds[_brokenFrontFixture] = "ChairHit3";
    addToBeginContact(_brokenFrontFixture);
    _rearContactFixtures.clear();
    for (b2Fixture* f = _rearBody->GetFixtureList(); f; f = f->GetNext()) {
        f->SetUserData(this);
        addToPostSolve(f);
        _rearContactFixtures.push_back(f);
        _contactAddSounds[f] = "ChairHit2";
        addToBeginContact(f);
    }
    _brokenRearFixture = _rearContactFixtures.empty() ? nullptr : _rearContactFixtures[0];

    b2Vec2 centre = _mowerBody->GetWorldCenter();
    shardBurst(centre, 30, Color4B(44, 90, 50, 255));
    createPositionSound("MetalSmashHeavy3", Vec2(centre.x, centre.y), 1.0f, false);

    // Wheels go onto the halves where their shocks were (Flash: shock joint anchor 1).
    b2Vec2 frontAnchor = _frontShockJoint->GetAnchorA();
    b2Vec2 backAnchor = _backShockJoint->GetAnchorA();
    world->DestroyJoint(_frontWheelJoint);
    world->DestroyJoint(_backWheelJoint);
    _frontWheelJoint = _backWheelJoint = nullptr;
    _frontShockJoint = _backShockJoint = nullptr;
    getLevel()->removeFromPaintBody(_mowerBody);
    _mowerSprite->setVisible(false);
    _bladeCoverSprite->setVisible(false);
    _shockNode->clear();
    world->DestroyBody(_mowerBody);
    world->DestroyBody(_frontShockBody);
    world->DestroyBody(_backShockBody);
    _mowerBody = _frontShockBody = _backShockBody = nullptr;
    _frontFixture = _rearFixture = _topFixture = _seatFixture = _bladeFixture = nullptr;
    _clearanceFixture = _baseFixture = _shaftFixture = nullptr;

    b2RevoluteJointDef wheel;
    wheel.Initialize(_frontBody, _frontWheelBody, frontAnchor);
    _frontWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&wheel));
    wheel.Initialize(_rearBody, _backWheelBody, backAnchor);
    _backWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&wheel));
}

// Flash frontSmash: the hood breaks into five pieces, the front wheel comes off.
void LawnMower::frontSmash()
{
    b2Body* from = _frontBody;
    for (b2Fixture* f = from->GetFixtureList(); f; f = f->GetNext()) {
        removePostSolve(f);
        removeBeginContact(f);
    }
    std::vector<b2Vec2> handlePolygon = verticesOf(_handleFixture);
    auto quad = [this](const char* prefix) {
        std::vector<b2Vec2> v;
        for (int i = 1; i <= 4; i++) {
            v.push_back(toB2(point(std::string(prefix) + patch::to_string(i))));
        }
        return v;
    };
    makePiece(from, {handlePolygon, quad("f1vert")}, "front1");
    makePiece(from, {quad("f2vert")}, "front2");
    makePiece(from, {quad("f3vert")}, "front3");
    makePiece(from, {quad("f4vert")}, "front4");
    makePiece(from, {quad("f5vert")}, "front5");
    shardBurst(from->GetWorldCenter(), 30, Color4B(44, 90, 50, 255));
    b2Vec2 c = from->GetWorldCenter();
    createPositionSound("MetalSmashHeavy2", Vec2(c.x, c.y), 1.0f, false);
    for (auto it = _pieces.begin(); it != _pieces.end(); ++it) {
        if (it->body == from) {
            it->sprite->removeFromParent();
            _pieces.erase(it);
            break;
        }
    }
    getWorld()->DestroyBody(from);  // its wheel joint goes with it
    _frontBody = nullptr;
    _frontWheelJoint = nullptr;
    _brokenFrontFixture = _handleFixture = nullptr;
}

// Flash rearSmash: deck, blade guard, body and seat fly apart, the back wheel comes off.
void LawnMower::rearSmash()
{
    b2Body* from = _rearBody;
    for (b2Fixture* f : _rearContactFixtures) {
        removePostSolve(f);
        removeBeginContact(f);
    }
    _rearContactFixtures.clear();
    ValueMap baseData = shape("baseShape");
    Vec2 basePos = PointFromString(baseData.at("pos").asString());
    Size baseSize = SizeFromString(baseData.at("size").asString());
    b2PolygonShape baseBox;
    baseBox.SetAsBox(baseSize.width, baseSize.height, toB2(basePos), baseData.at("rot").asFloat());
    std::vector<b2Vec2> base(baseBox.m_vertices, baseBox.m_vertices + baseBox.m_count);
    std::vector<b2Vec2> r1, rear, top, seat, seat2;
    for (int i = 0; i < 4; i++) {
        r1.push_back(toB2(point("r1vert" + patch::to_string(i))));
    }
    for (int i = 1; i <= 5; i++) {
        rear.push_back(toB2(point("rearVert" + patch::to_string(i))));
        top.push_back(toB2(point("topVert" + patch::to_string(i))));
    }
    for (int i = 1; i <= 4; i++) {
        seat.push_back(toB2(point("backVert" + patch::to_string(i))));
        seat2.push_back(toB2(point("seatVert" + patch::to_string(i))));
    }
    makePiece(from, {base}, "rear3");
    makePiece(from, {r1}, "rear4");
    makePiece(from, {rear, top}, "rear2");
    makePiece(from, {seat, seat2}, "rear1");
    shardBurst(from->GetWorldCenter(), 30, Color4B(44, 90, 50, 255));
    b2Vec2 c = from->GetWorldCenter();
    createPositionSound("MetalSmashHeavy", Vec2(c.x, c.y), 1.0f, false);
    for (auto it = _pieces.begin(); it != _pieces.end(); ++it) {
        if (it->body == from) {
            it->sprite->removeFromParent();
            _pieces.erase(it);
            break;
        }
    }
    getWorld()->DestroyBody(from);
    _rearBody = nullptr;
    _backWheelJoint = nullptr;
    _brokenRearFixture = nullptr;
}

void LawnMower::debugFunction(int value)
{
    if (!_mowerSmashed) {
        mowerSmash();
    }
}

// ---- sounds ---------------------------------------------------------------------------------

void LawnMower::startLoop(Sound*& sound, const std::string& name, float volume, float fade)
{
    if (sound) {
        sound->fadeTo(volume, fade, false);
        return;
    }
    sound = createBodySound(name, _backWheelBody, 1.0f, true);
    if (sound) {
        Sound** slot = &sound;
        sound->setFinishCallback([slot](int&) { *slot = nullptr; });
        sound->setMaxVolume(0.0f);
        sound->fadeTo(volume, fade, false);
    }
}

void LawnMower::fadeLoop(Sound*& sound, float volume, float time, bool stop)
{
    if (!sound) {
        return;
    }
    sound->fadeTo(volume, time, stop);
    if (stop) {
        sound->setFinishCallback(nullptr);
        sound = nullptr;
    }
}

// ---- QOL (PC addition): re-grab vehicle (src/game/vehicles/Vehicle.h) ---------------------------

b2Body* LawnMower::qolFrameBody()
{
    return _mowerBody;
}

// checkStateOfCharacter: off once both hands and both feet have let go.
bool LawnMower::qolCanRemount(CharacterB2D* character)
{
    return character == _rider && !_mowerSmashed && _mowerBody &&
           !(character->qolLostLowerArm(1) && character->qolLostLowerArm(2) &&
             character->qolLostLowerLeg(1) && character->qolLostLowerLeg(2));
}

// ejectCharacter: _ejected, controls nulled, the legs solid again (addCharacter makes the
// remaining ones sensors again), the shocks locked (as while riding). The mower keeps the riders'
// group, so his hands reach it through qolTouchedBody (no contact).
void LawnMower::qolRemount(CharacterB2D* character)
{
    _ejected = false;
    qolRestoreFilters(character);
    qolMount(character, [this, character]() { addCharacter(character); });
    qolReplayInjuries(character);
}
