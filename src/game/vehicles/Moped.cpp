#include "Moped.h"

#include <cmath>
#include <string>

#include "CharacterB2D.h"
#include "GameplayControls.h"
#include "LevelB2D.h"
#include "Session.h"
#include "Sound.h"
#include "online/FlashPhysics.h"  // ONLINE (PC addition)
#include "platform/compat/Box2DFloat.h"

USING_NS_CC;

// @005ee190
Moped::Moped()
    : _wheelSmashLimit(200.0f),
      _frameSmashLimit(200.0f),
      _impulseMagnitudeMax(0.75f),
      _impulseOffset(1.0f),
      _maxSpinAV(5.0f)
{
}

// @005ee288 (D1), @005ee2c4 (D0)
Moped::~Moped()
{
}

// @005ee2e8
bool Moped::init(Vec2 position, std::string name, int groupID)
{
    _engineSound = nullptr;
    _maxSpeed = -30.0f;
    _accelStep = 0.5f;
    _maxTorque = 30.0f;
    _boosting = false;
    _boostVal = 0.0f;
    _boostMax = 50;
    _boostStepUp = 1;
    _boostImpulse = 2;
    _boostStepDown = 0.25f;
    _defaultPitch = 1.0f;
    _passengerEjected = false;
    _wheelSoundName = "BikeLoop1";

    bool result = Vehicle::init(position, name, groupID);
    if (result) {
        getLevel()->addToFrameActions(this);
    }
    return result;
}

// @005ee41c
void Moped::lockWheels()
{
    _backWheelJoint->EnableLimit(true);
    _backWheelJoint->SetLimits(0.0f, 0.0f);
}

// @005ee450
void Moped::createSprites()
{
    Node* characterForeground = getSession()->getCharacterForeground();

    _frontSpokesSprite = Sprite::createWithSpriteFrameName(_name + "_spokes.png");
    _backSpokesSprite = Sprite::createWithSpriteFrameName(_name + "_spokes.png");
    characterForeground->addChild(_frontSpokesSprite);
    characterForeground->addChild(_backSpokesSprite);

    _frontWheelSprite = Sprite::createWithSpriteFrameName(_name + "_frontWheel.png");
    _backWheelSprite = Sprite::createWithSpriteFrameName(_name + "_backWheel.png");
    characterForeground->addChild(_frontWheelSprite);
    characterForeground->addChild(_backWheelSprite);

    _frameSprite = Sprite::createWithSpriteFrameName(_name + "_frame.png");
    _frameSprite->setAnchorPoint(Vec2(0.45f, 1.305f));
    characterForeground->addChild(_frameSprite);
}

// @005ee958
void Moped::customizeControls()
{
    if (getSession()->getControls()) {
        getSession()->getLevel()->addToFrameActions(this);
    }
}

// @005ee99c
void Moped::createBodies()
{
    b2World* world = getWorld();

    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    ValueMap engine = bodies.at("engine").asValueMap();
    ValueMap middle = bodies.at("middle").asValueMap();
    ValueMap rear = bodies.at("rear").asValueMap();
    ValueMap backWheelShape = bodies.at("backWheelShape").asValueMap();
    ValueMap frontWheelShape = bodies.at("frontWheelShape").asValueMap();
    ValueMap fork = bodies.at("fork").asValueMap();
    ValueMap tank = bodies.at("tank").asValueMap();
    ValueMap seat = bodies.at("seat").asValueMap();

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position.Set(_origin.x, _origin.y);
    _frameBody = world->CreateBody(&bodyDef);

    b2FixtureDef fixtureDef;
    fixtureDef.userData = this;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 3.0f;
    fixtureDef.filter = _zeroFilter;
    _engineFixture = createFixture(_frameBody, fixtureDef, &engine, false, false);
    _middleFixture = createFixture(_frameBody, fixtureDef, &middle, false, false);
    _rearFixture = createFixture(_frameBody, fixtureDef, &rear, false, false);

    // The binary re-reads _zeroFilter before the wheels and again before the fork group.
    fixtureDef.filter = _zeroFilter;
    // ONLINE (PC addition): the browser game's wheels are rubber (MopedGuy.createBodies).
    if (online::browserPhysicsWanted()) {
        fixtureDef.density = 5.0f;
        fixtureDef.friction = 1.0f;
        fixtureDef.restitution = 0.3f;
    }
    Vec2 wheelPosition = PointFromString(backWheelShape["pos"].asString()) + _origin;
    bodyDef.position.Set(wheelPosition.x, wheelPosition.y);
    b2Body* backWheelBody = world->CreateBody(&bodyDef);
    _backWheelFixture = createFixture(backWheelBody, fixtureDef, &backWheelShape, false, false);
    backWheelBody->ResetMassData();

    wheelPosition = PointFromString(frontWheelShape["pos"].asString()) + _origin;
    bodyDef.position.Set(wheelPosition.x, wheelPosition.y);
    b2Body* frontWheelBody = world->CreateBody(&bodyDef);
    _frontWheelFixture = createFixture(frontWheelBody, fixtureDef, &frontWheelShape, false, false);
    frontWheelBody->ResetMassData();

    fixtureDef.filter = _zeroFilter;
    fixtureDef.density = 3.0f;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    _forkFixture = createFixture(_frameBody, fixtureDef, &fork, false, false);
    _tankFixture = createFixture(_frameBody, fixtureDef, &tank, false, false);
    _seatFixture = createFixture(_frameBody, fixtureDef, &seat, false, false);
    _frameBody->ResetMassData();

    addToPostSolve(_rearFixture);
    addToPostSolve(_engineFixture);
    addToPostSolve(_tankFixture);
    addToPostSolve(_forkFixture);
    addToPostSolve(_seatFixture);
    addToBeginContact(_rearFixture);
    addToBeginContact(_engineFixture);
    addToBeginContact(_tankFixture);
    addToBeginContact(_forkFixture);

    addToPostSolve(_frontWheelFixture);
    addToBeginContact(_frontWheelFixture);
    addToEndContact(_frontWheelFixture);
    addToPostSolve(_backWheelFixture);
    addToBeginContact(_backWheelFixture);
    addToEndContact(_backWheelFixture);

    _frameBody->ResetMassData();
    _frameBody->SetUserData(_frameSprite);
    getLevel()->addToPaintBody(_frameBody);

    if (getSession()->getMode() == SessionModeGameplay) {
        // The engine loop follows the front wheel body (iOS used the frame body).
        _engineSound = createBodySound("MopedLoop2", frontWheelBody, 1.0f, true);
        // PC: no sound without an audio device; everything else already checks _engineSound.
        if (_engineSound) {
            _engineSound->setMaxVolume(0.0f);
            _engineSound->fadeTo(0.5f, 0.5f, false);
        }
    }
    getLevel()->addToPaintItem(this);
}

// @005ef728
void Moped::createJoints()
{
    b2World* world = getWorld();

    b2RevoluteJointDef jointDef;
    jointDef.maxMotorTorque = _maxTorque;

    b2Body* backWheelBody = _backWheelFixture->GetBody();
    b2Vec2 anchor = backWheelBody->GetPosition();
    jointDef.Initialize(_frameBody, backWheelBody, anchor);
    _backWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addWheelJoint(_backWheelJoint, backWheelBody);

    b2Body* frontWheelBody = _frontWheelFixture->GetBody();
    anchor = frontWheelBody->GetPosition();
    jointDef.Initialize(_frameBody, frontWheelBody, anchor);
    _frontWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addWheelJoint(_frontWheelJoint, frontWheelBody);
}

// @005ef84c
void Moped::createDictionaries()
{
    _contactImpulseDict[_tankFixture] = _frameSmashLimit;
    _contactImpulseDict[_frontWheelFixture] = _wheelSmashLimit;
    _contactImpulseDict[_backWheelFixture] = _wheelSmashLimit;

    _contactAddSounds[_backWheelFixture] = "TireHit1";
    _contactAddSounds[_frontWheelFixture] = "TireHit2";
    _contactAddSounds[_tankFixture] = "ChairHit1";
    _contactAddSounds[_forkFixture] = "ChairHit1";
    _contactAddSounds[_engineFixture] = "ChairHit2";
    _contactAddSounds[_rearFixture] = "ChairHit3";
}

// @005efe4c
void Moped::frameAction()
{
    // Engine pitch follows the back wheel speed (1 .. 1.65).
    _targetPitch = fminf(fabsf(owb2::jointSpeed(_backWheelJoint)) / 60.0f, 1.0f) * 0.65f + 1.0f;
    if (_engineSound) {
        float pitch = (_targetPitch - _engineSound->getDefaultPitch()) * 0.5f +
                      _engineSound->getDefaultPitch();
        _engineSound->setDefaultPitch(pitch);
    }

    if (_boosting) {
        _boostVal = fminf(_boostMax, _boostVal + _boostStepUp);
        if (_boostVal < _boostMax) {
            float angle = _frameBody->GetAngle();
            _frameBody->ApplyLinearImpulse(_boostImpulse * b2Vec2(cosf(angle), sinf(angle)),
                                           _frameBody->GetWorldCenter(), true);
        }
    } else {
        _targetPitch = 1.0f;
        _boostVal = fmaxf(_boostVal - _boostStepDown, 0.0f);
    }

    GameplayControls* controls = getSession()->getControls();
    if (controls) {
        controls->setMeterPercentage(1.0f - _boostVal / _boostMax);
    }
}

// @005f0010
void Moped::addDriver(CharacterB2D* character)
{
    _driver = character;
    attachCharacter(character);
}

// @005f0018
void Moped::attachCharacter(CharacterB2D* character)
{
    b2World* world = getWorld();

    // Joint limits relative to the current pose: hips -10..110 degrees, elbows 0..90, neck 0..20.
    // QOL (PC addition): qolLimb - a re-mounted driver (re-grab vehicle) may have lost limbs.
    // ONLINE (PC addition): the browser game works the hip limits out but never sets them.
    const bool hips = !online::browserPhysicsWanted();
    float relativeAngle = character->getUpperLeg1Body()->GetAngle() - character->getPelvisBody()->GetAngle();
    if (hips && qolLimb(character, character->getHipJoint1()))
    character->getHipJoint1()->SetLimits(CC_DEGREES_TO_RADIANS(-10) - relativeAngle,
                                         CC_DEGREES_TO_RADIANS(110) - relativeAngle);
    relativeAngle = character->getUpperLeg2Body()->GetAngle() - character->getPelvisBody()->GetAngle();
    if (hips && qolLimb(character, character->getHipJoint2()))
    character->getHipJoint2()->SetLimits(CC_DEGREES_TO_RADIANS(-10) - relativeAngle,
                                         CC_DEGREES_TO_RADIANS(110) - relativeAngle);
    relativeAngle = character->getLowerArm1Body()->GetAngle() - character->getUpperArm1Body()->GetAngle();
    if (qolLimb(character, character->getElbowJoint1()))
    character->getElbowJoint1()->SetLimits(CC_DEGREES_TO_RADIANS(0) - relativeAngle,
                                           CC_DEGREES_TO_RADIANS(90) - relativeAngle);
    relativeAngle = character->getLowerArm2Body()->GetAngle() - character->getUpperArm2Body()->GetAngle();
    if (qolLimb(character, character->getElbowJoint2()))
    character->getElbowJoint2()->SetLimits(CC_DEGREES_TO_RADIANS(0) - relativeAngle,
                                           CC_DEGREES_TO_RADIANS(90) - relativeAngle);
    relativeAngle = character->getHeadBody()->GetAngle() - character->getChestBody()->GetAngle();
    character->getNeckJoint()->SetLimits(CC_DEGREES_TO_RADIANS(0) - relativeAngle,
                                         CC_DEGREES_TO_RADIANS(20) - relativeAngle);

    b2RevoluteJointDef jointDef;
    jointDef.maxMotorTorque = _maxTorque;

    // Pelvis -> frame. (The original writes _bodyVehicleJointDict directly, not through the
    // virtual addBodyVehicleJoint.)
    b2Vec2 anchor = character->getPelvisBody()->GetPosition();
    jointDef.Initialize(_frameBody, character->getPelvisBody(), anchor);
    _bodyVehicleJointDict[character->getPelvisBody()] = world->CreateJoint(&jointDef);

    // Hands: the driver holds the handlebar (frame), the passenger holds the driver's chest. The
    // anchor is a point along the limb computed from its first polygon's vertices.
    b2PolygonShape* shape = static_cast<b2PolygonShape*>(
        character->getLowerArm1Body()->GetFixtureList()->GetShape());
    float length = shape->m_vertices[1].x - shape->m_vertices[0].x;
    b2Body* handBody = (_driver == character) ? _frameBody : _driver->getChestBody();
    anchor = character->getLowerArm1Body()->GetWorldPoint(
        b2Vec2(0.0f, length / 2 + shape->m_vertices[0].y));
    // ONLINE (PC addition): the browser riders hold on with both hands at one point, and stand
    // with both feet on another.
    b2Vec2 flashHand, flashFoot;
    const bool flashHands = character->onlineFlashAnchor("handleAnchor", &flashHand);
    const bool flashFeet = character->onlineFlashAnchor("footAnchor", &flashFoot);
    if (flashHands) anchor = flashHand;
    jointDef.Initialize(handBody, character->getLowerArm1Body(), anchor);
    _bodyVehicleJointDict[character->getLowerArm1Body()] = world->CreateJoint(&jointDef);

    shape = static_cast<b2PolygonShape*>(character->getLowerArm2Body()->GetFixtureList()->GetShape());
    length = shape->m_vertices[1].x - shape->m_vertices[0].x;
    anchor = character->getLowerArm2Body()->GetWorldPoint(
        b2Vec2(0.0f, shape->m_vertices[0].y + length / 2));
    if (flashHands) anchor = flashHand;  // ONLINE (PC addition)
    jointDef.Initialize(handBody, character->getLowerArm2Body(), anchor);
    _bodyVehicleJointDict[character->getLowerArm2Body()] = world->CreateJoint(&jointDef);

    // Feet -> frame.
    shape = static_cast<b2PolygonShape*>(character->getLowerLeg1Body()->GetFixtureList()->GetShape());
    length = shape->m_vertices[1].x - shape->m_vertices[0].x;
    anchor = character->getLowerLeg1Body()->GetWorldPoint(
        b2Vec2(0.0f, shape->m_vertices[0].y + length / 2));
    if (flashFeet) anchor = flashFoot;  // ONLINE (PC addition)
    jointDef.Initialize(_frameBody, character->getLowerLeg1Body(), anchor);
    _bodyVehicleJointDict[character->getLowerLeg1Body()] = world->CreateJoint(&jointDef);

    shape = static_cast<b2PolygonShape*>(character->getLowerLeg2Body()->GetFixtureList()->GetShape());
    length = shape->m_vertices[1].x - shape->m_vertices[0].x;
    anchor = character->getLowerLeg2Body()->GetWorldPoint(
        b2Vec2(0.0f, shape->m_vertices[0].y + length / 2));
    if (flashFeet) anchor = flashFoot;  // ONLINE (PC addition)
    jointDef.Initialize(_frameBody, character->getLowerLeg2Body(), anchor);
    _bodyVehicleJointDict[character->getLowerLeg2Body()] = world->CreateJoint(&jointDef);

    if (_driver == character) {
        // The driver's legs collide with nothing while seated (mask restored on eject).
        b2Filter filter = character->getUpperLeg1Body()->GetFixtureList()->GetFilterData();
        _previousMaskBits = filter.maskBits;
        filter.maskBits = 0;
        character->getUpperLeg1Body()->GetFixtureList()->SetFilterData(filter);
        filter = character->getLowerLeg1Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = 0;
        character->getLowerLeg1Body()->GetFixtureList()->SetFilterData(filter);
        filter = character->getUpperLeg2Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = 0;
        character->getUpperLeg2Body()->GetFixtureList()->SetFilterData(filter);
        filter = character->getLowerLeg2Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = 0;
        character->getLowerLeg2Body()->GetFixtureList()->SetFilterData(filter);
    } else {
        // The passenger's legs are sensors; their contacts with the seat are counted
        // (beginContact/endContact) so they become solid once both legs are clear.
        character->getUpperLeg1Body()->GetFixtureList()->SetSensor(true);
        character->getLowerLeg1Body()->GetFixtureList()->SetSensor(true);
        character->getUpperLeg2Body()->GetFixtureList()->SetSensor(true);
        character->getLowerLeg2Body()->GetFixtureList()->SetSensor(true);
        if (online::browserPhysicsWanted()) {
            character->getLowerArm1Body()->GetFixtureList()->SetSensor(true);
            character->getLowerArm2Body()->GetFixtureList()->SetSensor(true);
        }
        _leg1Contacts = 0;
        _leg2Contacts = 0;
        character->getUpperLeg1Body()->GetFixtureList()->SetUserData(this);
        character->getUpperLeg2Body()->GetFixtureList()->SetUserData(this);
        addToBeginContact(character->getUpperLeg1Body()->GetFixtureList());
        addToEndContact(character->getUpperLeg1Body()->GetFixtureList());
        addToBeginContact(character->getUpperLeg2Body()->GetFixtureList());
        addToEndContact(character->getUpperLeg2Body()->GetFixtureList());
    }
}

// @005f0978
void Moped::addPassenger(CharacterB2D* character)
{
    _passenger = character;
    attachCharacter(character);
}

// @005f0980
void Moped::leg1Stuck()
{
    _leg1Contacts++;
}

// @005f0990
void Moped::leg1Free()
{
    _leg1Contacts--;
    if (_passengerEjected) {
        checkLegsFree();
    }
}

// @005f09ac
void Moped::checkLegsFree()
{
    if (_leg1Contacts == 0 && _leg2Contacts == 0 && _passenger) {
        removeBeginContact(_passenger->getUpperLeg1Body()->GetFixtureList());
        removeBeginContact(_passenger->getUpperLeg2Body()->GetFixtureList());
        removeEndContact(_passenger->getUpperLeg1Body()->GetFixtureList());
        removeEndContact(_passenger->getUpperLeg2Body()->GetFixtureList());
        getLevel()->addToSingleActions(this);
    }
}

// @005f0a44
void Moped::leg2Stuck()
{
    _leg2Contacts++;
}

// @005f0a54
void Moped::leg2Free()
{
    _leg2Contacts--;
    if (_passengerEjected) {
        checkLegsFree();
    }
}

// @005f0a70
void Moped::singleAction()
{
    if (_passenger) {
        _passenger->getUpperLeg1Body()->GetFixtureList()->SetSensor(false);
        _passenger->getLowerLeg1Body()->GetFixtureList()->SetSensor(false);
        _passenger->getUpperLeg2Body()->GetFixtureList()->SetSensor(false);
        _passenger->getLowerLeg2Body()->GetFixtureList()->SetSensor(false);
        solidPassengerArms(true, true);
    }
}

// ONLINE (PC addition): MopedGirl.legsFree, neckBreak, elbowBreak and shoulderBreak in the browser
// game make her forearms solid again.
void Moped::solidPassengerArms(bool arm1, bool arm2)
{
    if (!_passenger || !online::browserPhysicsWanted()) {
        return;
    }
    b2Body* arms[2] = {arm1 ? _passenger->getLowerArm1Body() : nullptr,
                       arm2 ? _passenger->getLowerArm2Body() : nullptr};
    for (b2Body* arm : arms) {
        b2Fixture* fixture = arm ? arm->GetFixtureList() : nullptr;
        if (fixture && fixture->IsSensor()) {
            fixture->SetSensor(false);
            fixture->Refilter();
        }
    }
}

void Moped::handleInjury(CharacterInjury injury, CharacterB2D* character)
{
    Vehicle::handleInjury(injury, character);
    if (character != _passenger) {
        return;
    }
    switch (injury) {
    case CharacterInjuryShoulder1Break:
    case CharacterInjuryElbow1Break:
        solidPassengerArms(true, false);
        break;
    case CharacterInjuryShoulder2Break:
    case CharacterInjuryElbow2Break:
        solidPassengerArms(false, true);
        break;
    case CharacterInjuryNeckBreak:
        solidPassengerArms(true, true);
        break;
    default:
        break;
    }
}

// @005f0ae8
void Moped::handleContactResults()
{
    // Only the impulse decides; the smash handlers read the stored normal (frameSmash ignores
    // both arguments, so the optimiser dropped them at this call site).
    if (_contactResultBufferDict[_tankFixture].impulse != 0.0f) {
        VehicleContact& contact = _contactResultBufferDict[_tankFixture];
        frameSmash(contact.impulse, contact.normal);
    }
    if (_contactResultBufferDict[_frontWheelFixture].impulse != 0.0f) {
        VehicleContact& contact = _contactResultBufferDict[_frontWheelFixture];
        frontWheelSmash(contact.impulse, contact.normal);
    }
    if (_contactResultBufferDict[_backWheelFixture].impulse != 0.0f) {
        VehicleContact& contact = _contactResultBufferDict[_backWheelFixture];
        backWheelSmash(contact.impulse, contact.normal);
    }
    _contactResultBufferDict.clear();
}

// @005f0f10
void Moped::frameSmash(float impulse, b2Vec2 normal)
{
    removeBeginContact(_tankFixture);
    removeBeginContact(_forkFixture);
    removeBeginContact(_engineFixture);
    removeBeginContact(_rearFixture);
    removePostSolve(_tankFixture);
    removePostSolve(_forkFixture);
    removePostSolve(_engineFixture);
    removePostSolve(_rearFixture);
    removePostSolve(_seatFixture);

    _vehicleSmashed = true;
    ejectAllCharacters();
    _frameSprite->removeFromParentAndCleanup(false);
    getLevel()->removeFromPaintBody(_frameBody);
    getWorld()->DestroyJoint(_frontWheelJoint);
    getWorld()->DestroyJoint(_backWheelJoint);

    // The frame breaks into one body per frame fixture, with the broken-part art.
    b2Shape* forkShape = _forkFixture->GetShape();
    b2Shape* tankShape = _tankFixture->GetShape();
    b2Shape* rearShape = _rearFixture->GetShape();
    b2Shape* seatShape = _seatFixture->GetShape();
    b2Shape* engineShape = _engineFixture->GetShape();
    b2Shape* middleShape = _middleFixture->GetShape();

    Node* vehicleForeground = getSession()->getVehicleForeground();
    Sprite* forkSprite = Sprite::createWithSpriteFrameName(_name + "_fork.png");
    Sprite* seatSprite = Sprite::createWithSpriteFrameName(_name + "_seat.png");
    Sprite* middleSprite = Sprite::createWithSpriteFrameName(_name + "_middle.png");
    Sprite* engineSprite = Sprite::createWithSpriteFrameName(_name + "_engine.png");
    Sprite* rearSprite = Sprite::createWithSpriteFrameName(_name + "_rear.png");
    Sprite* tankSprite = Sprite::createWithSpriteFrameName(_name + "_tank.png");
    forkSprite->setAnchorPoint(Vec2(-0.775f, 1.35f));
    middleSprite->setAnchorPoint(Vec2(0.79f, 2.055f));
    rearSprite->setAnchorPoint(Vec2(1.305f, 3.075f));
    engineSprite->setAnchorPoint(Vec2(0.15f, 3.06f));
    tankSprite->setAnchorPoint(Vec2(-0.15f, 1.805f));
    seatSprite->setAnchorPoint(Vec2(0.865f, 2.95f));
    vehicleForeground->addChild(forkSprite);
    vehicleForeground->addChild(seatSprite);
    vehicleForeground->addChild(middleSprite);
    vehicleForeground->addChild(engineSprite);
    vehicleForeground->addChild(rearSprite);
    vehicleForeground->addChild(tankSprite);

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = _frameBody->GetPosition();
    bodyDef.angle = _frameBody->GetAngle();

    // Every piece gets the fork fixture's material.
    b2FixtureDef fixtureDef;
    fixtureDef.density = _forkFixture->GetDensity();
    fixtureDef.friction = _forkFixture->GetFriction();
    fixtureDef.restitution = _forkFixture->GetRestitution();
    fixtureDef.filter = _zeroFilter;

    b2Vec2 linearVelocity = _frameBody->GetLinearVelocity();
    float angularVelocity = _frameBody->GetAngularVelocity();

    bodyDef.userData = forkSprite;
    fixtureDef.shape = forkShape;
    b2Body* body = getWorld()->CreateBody(&bodyDef);
    body->CreateFixture(&fixtureDef);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    bodyDef.userData = seatSprite;
    fixtureDef.shape = seatShape;
    body = getWorld()->CreateBody(&bodyDef);
    body->CreateFixture(&fixtureDef);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    bodyDef.userData = middleSprite;
    fixtureDef.shape = middleShape;
    body = getWorld()->CreateBody(&bodyDef);
    body->CreateFixture(&fixtureDef);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    bodyDef.userData = engineSprite;
    fixtureDef.shape = engineShape;
    body = getWorld()->CreateBody(&bodyDef);
    body->CreateFixture(&fixtureDef);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    bodyDef.userData = rearSprite;
    fixtureDef.shape = rearShape;
    body = getWorld()->CreateBody(&bodyDef);
    body->CreateFixture(&fixtureDef);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    bodyDef.userData = tankSprite;
    fixtureDef.shape = tankShape;
    body = getWorld()->CreateBody(&bodyDef);
    body->CreateFixture(&fixtureDef);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    getWorld()->DestroyBody(_frameBody);
    _frameBody = nullptr;
    getLevel()->removeFromActions(this);
    getLevel()->removeFromFrameActions(this);
    stopEngineSound();
    // Attached to the last piece (the tank).
    createBodySound("MetalSmashMedium", body, 1.0f, false);
}

// @005f1b68
void Moped::frontWheelSmash(float impulse, b2Vec2 normal)
{
    _contactResultBufferDict.erase(_frontWheelFixture);
    removePostSolve(_frontWheelFixture);

    Sprite* oldSprite = _frontWheelSprite;
    Node* parent = oldSprite->getParent();
    _frontWheelSprite = Sprite::createWithSpriteFrameName(_name + "_frontWheel_Broken.png");
    int zOrder = oldSprite->getLocalZOrder();
    oldSprite->removeFromParentAndCleanup(false);
    _frontSpokesSprite->removeFromParentAndCleanup(false);
    _frontSpokesSprite = nullptr;
    parent->addChild(_frontWheelSprite, zOrder - 1);

    b2Body* wheelBody = _frontWheelFixture->GetBody();
    wheelBody->SetUserData(_frontWheelSprite);

    // The round wheel becomes a flat box oriented along the hit normal.
    b2PolygonShape shape;
    float radius = _frontWheelFixture->GetShape()->m_radius;
    _frontWheelSprite->setRotation((atan2f(normal.y, normal.x) - wheelBody->GetAngle()) * 180 / M_PI);
    shape.SetAsBox(radius * 0.46f, radius * 0.92f);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.density = _frontWheelFixture->GetDensity();
    fixtureDef.friction = _frontWheelFixture->GetFriction();
    fixtureDef.restitution = _frontWheelFixture->GetRestitution();
    fixtureDef.filter = _ejected ? _defaultFilter : _zeroFilter;

    b2Body* body = _frontWheelFixture->GetBody();
    fixtureWillBeDestroyed(_frontWheelFixture);
    body->DestroyFixture(_frontWheelFixture);
    _frontWheelFixture = body->CreateFixture(&fixtureDef);
    body->ResetMassData();

    createBodySound("BikeTireSmash1", _frontWheelFixture->GetBody(), 1.0f, false);
}

// @005f1fc0
void Moped::backWheelSmash(float impulse, b2Vec2 normal)
{
    _contactResultBufferDict.erase(_backWheelFixture);
    removePostSolve(_backWheelFixture);

    Sprite* oldSprite = _backWheelSprite;
    Node* parent = oldSprite->getParent();
    _backWheelSprite = Sprite::createWithSpriteFrameName(_name + "_backWheel_broken.png");
    int zOrder = oldSprite->getLocalZOrder();
    oldSprite->removeFromParentAndCleanup(false);
    _backSpokesSprite->removeFromParentAndCleanup(false);
    _backSpokesSprite = nullptr;
    parent->addChild(_backWheelSprite, zOrder - 1);

    b2Body* wheelBody = _backWheelFixture->GetBody();
    wheelBody->SetUserData(_backWheelSprite);

    b2PolygonShape shape;
    float radius = _backWheelFixture->GetShape()->m_radius;
    _backWheelSprite->setRotation((atan2f(normal.y, normal.x) - wheelBody->GetAngle()) * 180 / M_PI);
    shape.SetAsBox(radius * 0.46f, radius * 0.92f);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.density = _backWheelFixture->GetDensity();
    fixtureDef.friction = _backWheelFixture->GetFriction();
    fixtureDef.restitution = _backWheelFixture->GetRestitution();
    fixtureDef.filter = _ejected ? _defaultFilter : _zeroFilter;

    b2Body* body = _backWheelFixture->GetBody();
    fixtureWillBeDestroyed(_backWheelFixture);
    body->DestroyFixture(_backWheelFixture);
    _backWheelFixture = body->CreateFixture(&fixtureDef);
    body->ResetMassData();

    createBodySound("BikeTireSmash1", _backWheelFixture->GetBody(), 1.0f, false);
}

// @005f2418
void Moped::stopEngineSound()
{
    if (_engineSound) {
        _engineSound->stop();
        _engineSound = nullptr;
    }
}

// @005f244c  (no callers in 1.1.3; postSolve records the tank impulse itself)
void Moped::handleFramePostSolve(VehicleContact contact)
{
    if (contact.impulse > _contactImpulseDict[_tankFixture]) {
        if (_contactResultBufferDict[_tankFixture].impulse == 0.0f ||
            contact.impulse > _contactResultBufferDict[_tankFixture].impulse) {
            _contactResultBufferDict[_tankFixture] = contact;
        }
    }
}

// @005f275c
void Moped::handleUpperLeg1Injury(CharacterB2D* character)
{
    Vehicle::handleUpperLeg1Injury(character);
    if (_passenger == character) {
        character->getUpperLeg1Body()->GetFixtureList()->SetSensor(false);
        character->getLowerLeg1Body()->GetFixtureList()->SetSensor(false);
    }
}

// @005f27bc
void Moped::handleUpperLeg2Injury(CharacterB2D* character)
{
    Vehicle::handleUpperLeg2Injury(character);
    if (_passenger == character) {
        character->getUpperLeg2Body()->GetFixtureList()->SetSensor(false);
        character->getLowerLeg2Body()->GetFixtureList()->SetSensor(false);
    }
}

// @005f281c
void Moped::handleLowerLeg1Injury(CharacterB2D* character)
{
    Vehicle::handleLowerLeg1Injury(character);
    if (_passenger == character) {
        character->getLowerLeg1Body()->GetFixtureList()->SetSensor(false);
    }
}

// @005f2868
void Moped::handleLowerLeg2Injury(CharacterB2D* character)
{
    Vehicle::handleLowerLeg2Injury(character);
    if (_passenger == character) {
        character->getLowerLeg2Body()->GetFixtureList()->SetSensor(false);
    }
}

// @005f28b4
void Moped::checkStateOfCharacter(CharacterB2D* character)
{
    unsigned char brokenJoints = 0;
    if (!_bodyVehicleJointDict[character->getLowerLeg1Body()]) {
        brokenJoints++;
    }
    if (!_bodyVehicleJointDict[character->getLowerLeg2Body()]) {
        brokenJoints++;
    }
    if (brokenJoints == 2) {
        // Both feet off: release the pelvis too.
        if (_bodyVehicleJointDict[character->getPelvisBody()]) {
            refilterLegs(_previousMaskBits);
            getWorld()->DestroyJoint(_bodyVehicleJointDict[character->getPelvisBody()]);
            _bodyVehicleJointDict.erase(character->getPelvisBody());
            character->resetJointLimits();
        }
    }
    if (!_bodyVehicleJointDict[character->getLowerArm1Body()]) {
        brokenJoints++;
    }
    if (!_bodyVehicleJointDict[character->getLowerArm2Body()]) {
        brokenJoints++;
    }
    if (brokenJoints > 2) {
        ejectCharacter(character);
    }
}

// @005f2df0
// Sets the maskBits of the driver's (_characters[0]) four leg fixtures; 0 remembers the old mask.
void Moped::refilterLegs(unsigned int maskBits)
{
    CharacterB2D* driver = _characters[0];
    b2Filter filter = driver->getUpperLeg1Body()->GetFixtureList()->GetFilterData();
    if (maskBits == 0) {
        _previousMaskBits = filter.maskBits;
    }
    filter.maskBits = maskBits;
    driver->getUpperLeg1Body()->GetFixtureList()->SetFilterData(filter);
    filter = driver->getLowerLeg1Body()->GetFixtureList()->GetFilterData();
    filter.maskBits = maskBits;
    driver->getLowerLeg1Body()->GetFixtureList()->SetFilterData(filter);
    filter = driver->getUpperLeg2Body()->GetFixtureList()->GetFilterData();
    filter.maskBits = maskBits;
    driver->getUpperLeg2Body()->GetFixtureList()->SetFilterData(filter);
    filter = driver->getLowerLeg2Body()->GetFixtureList()->GetFilterData();
    filter.maskBits = maskBits;
    driver->getLowerLeg2Body()->GetFixtureList()->SetFilterData(filter);
}

// @005f2f24
void Moped::ejectAllCharacters()
{
    getLevel()->removeFromFrameActions(this);
    Vehicle::ejectAllCharacters();
}

// @005f2f50
bool Moped::ejectCharacter(CharacterB2D* character)
{
    if (_driver == character) {
        // The passenger was holding on to the driver.
        if (_passenger) {
            destroyJointsForBody(_passenger->getLowerArm1Body());
            destroyJointsForBody(_passenger->getLowerArm2Body());
        }
        b2Filter filter = _driver->getUpperLeg1Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = _previousMaskBits;
        _driver->getUpperLeg1Body()->GetFixtureList()->SetFilterData(filter);
        filter = _driver->getLowerLeg1Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = _previousMaskBits;
        _driver->getLowerLeg1Body()->GetFixtureList()->SetFilterData(filter);
        filter = _driver->getUpperLeg2Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = _previousMaskBits;
        _driver->getUpperLeg2Body()->GetFixtureList()->SetFilterData(filter);
        filter = _driver->getLowerLeg2Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = _previousMaskBits;
        _driver->getLowerLeg2Body()->GetFixtureList()->SetFilterData(filter);
        if (!_vehicleSmashed) {
            for (b2Fixture* fixture = _frameBody->GetFixtureList(); fixture; fixture = fixture->GetNext()) {
                fixture->SetFilterData(_zeroFilter);
            }
        }
    } else {
        _passengerEjected = true;
    }
    Vehicle::ejectCharacter(character);
    return true;
}

// @005f3120
void Moped::special1ButtonPressed()
{
    _boosting = true;
}

// @005f312c
void Moped::special1ButtonNull()
{
    _boosting = false;
}

// @005f3134
// Spins the frame forward: two opposite impulses one unit either side of the centre of mass,
// scaled down as the angular velocity approaches -_maxSpinAV.
void Moped::leanForwardButtonPressed()
{
    setCurrentPose(VehiclePoseLeanForward);

    float angularVelocity = _frameBody->GetAngularVelocity();
    float angle = _frameBody->GetAngle();
    float factor = (angularVelocity + _maxSpinAV) / _maxSpinAV;
    if (factor <= 0.0f) {
        factor = 0.0f;
    }
    if (factor > 1.0f) {
        factor = 1.0f;
    }
    float magnitude = _impulseMagnitudeMax * LevelItem::s_timeStepOverFlashTimeStep;
    b2Vec2 localCenter = _frameBody->GetLocalCenter();

    b2Vec2 impulse(sinf(angle) * magnitude * factor, -cosf(angle) * magnitude * factor);
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x + 1.0f, localCenter.y)), true);
    impulse.Set(sin(angle + M_PI) * magnitude * factor, -cos(angle + M_PI) * magnitude * factor);
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x - 1.0f, localCenter.y)), true);
}

// @005f3348
void Moped::leanBackButtonPressed()
{
    setCurrentPose(VehiclePoseLeanBack);

    float angularVelocity = _frameBody->GetAngularVelocity();
    float angle = _frameBody->GetAngle();
    float factor = fmin(fmax((angularVelocity - _maxSpinAV) / -_maxSpinAV, 0.0), 1.0);
    float magnitude = _impulseMagnitudeMax * LevelItem::s_timeStepOverFlashTimeStep;
    b2Vec2 localCenter = _frameBody->GetLocalCenter();

    b2Vec2 impulse(sinf(angle) * magnitude * factor, -cosf(angle) * magnitude * factor);
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x - 1.0f, localCenter.y)), true);
    impulse.Set(sin(angle + M_PI) * magnitude * factor, -cos(angle + M_PI) * magnitude * factor);
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x + 1.0f, localCenter.y)), true);
}

// @005f3564
void Moped::leanBackPose()
{
    CharacterB2D* driver = _characters[0];
    if (driver->getNeckJoint()) {
        driver->setJoint(driver->getNeckJoint(), 0.0f, 2.0f, 20.0f);
    }
    if (driver->getElbowJoint1()) {
        driver->setJoint(driver->getElbowJoint1(), -1.04f, 15.0f, 20.0f);
    }
    if (driver->getElbowJoint2()) {
        driver->setJoint(driver->getElbowJoint2(), -1.04f, 15.0f, 20.0f);
    }
}

// @005f3620
void Moped::leanForwardPose()
{
    CharacterB2D* driver = _characters[0];
    if (driver->getNeckJoint()) {
        driver->setJoint(driver->getNeckJoint(), -1.0f, 1.0f, 20.0f);
    }
    if (driver->getElbowJoint1()) {
        driver->setJoint(driver->getElbowJoint1(), 0.0f, 15.0f, 20.0f);
    }
    if (driver->getElbowJoint2()) {
        driver->setJoint(driver->getElbowJoint2(), 0.0f, 15.0f, 20.0f);
    }
}

// @005f36cc
void Moped::noLeanBackPose()
{
}

// @005f36d0
void Moped::noLeanForwardPose()
{
}

// @005f36d4
void Moped::paint()
{
    b2Vec2 position = _frontWheelFixture->GetBody()->GetPosition();
    _frontWheelSprite->setPosition(Vec2(position.x * getPtm(), position.y * getPtm()));
    Sprite* rotatingSprite = _frontWheelSprite;
    if (_frontSpokesSprite) {
        _frontSpokesSprite->setPosition(_frontWheelSprite->getPosition());
        rotatingSprite = _frontSpokesSprite;
    }
    rotatingSprite->setRotation(-CC_RADIANS_TO_DEGREES(_frontWheelFixture->GetBody()->GetAngle()));

    position = _backWheelFixture->GetBody()->GetPosition();
    _backWheelSprite->setPosition(Vec2(position.x * getPtm(), position.y * getPtm()));
    rotatingSprite = _backWheelSprite;
    if (_backSpokesSprite) {
        _backSpokesSprite->setPosition(_backWheelSprite->getPosition());
        rotatingSprite = _backSpokesSprite;
    }
    rotatingSprite->setRotation(-CC_RADIANS_TO_DEGREES(_backWheelFixture->GetBody()->GetAngle()));
}

// @005f3864  (no callers; a tail call in the binary)
void Moped::random()
{
    frameSmash(10000.0f, b2Vec2(0.0f, 0.0f));
}

// @005f3868
void Moped::die()
{
    if (_engineSound) {
        stopEngineSound();
    }
}

// @005f389c
void Moped::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (_passenger) {
        if (_passenger->getUpperLeg1Body()->GetFixtureList() == fixture && _seatFixture == otherFixture) {
            leg1Stuck();
        } else if (_passenger->getUpperLeg2Body()->GetFixtureList() == fixture &&
                   _seatFixture == otherFixture) {
            leg2Stuck();
        }
    }
    Vehicle::beginContact(fixture, otherFixture, contact);
}

// @005f3938
void Moped::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (_passenger) {
        if (_passenger->getUpperLeg1Body()->GetFixtureList() == fixture && _seatFixture == otherFixture) {
            leg1Free();
        } else if (_passenger->getUpperLeg2Body()->GetFixtureList() == fixture &&
                   _seatFixture == otherFixture) {
            leg2Free();
        }
    }
    Vehicle::endContact(fixture, otherFixture, contact);
}

// @005f39ec
void Moped::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                      const b2ContactImpulse* impulse)
{
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        maxImpulse = b2Max(maxImpulse, impulse->normalImpulses[1]);
    }

    if (_tankFixture == fixture) {
        // Frame: only the strongest impulse above the smash limit is kept (no normal).
        if (_contactImpulseDict[fixture] < maxImpulse) {
            if (_contactResultBufferDict[_tankFixture].impulse < maxImpulse) {
                _contactResultBufferDict[_tankFixture].impulse = maxImpulse;
            }
        }
    } else if (_frontWheelFixture == fixture || _backWheelFixture == fixture) {
        // Wheels: the latest impulse above the limit, with the manifold normal.
        if (_contactImpulseDict[fixture] < maxImpulse) {
            _contactResultBufferDict[fixture].impulse = maxImpulse;
            _contactResultBufferDict[fixture].normal = contact->GetManifold()->localNormal;
        }
    }
}

// @005f3e1c
void Moped::debugFunction(int value)
{
}

// ---------------------------------------------------------------------------------------------
// QOL (PC addition): re-grab vehicle (Vehicle.h). Not in the original. Only the driver gets back
// on; the passenger, once off, stays off.
// ---------------------------------------------------------------------------------------------

b2Body* Moped::qolFrameBody()
{
    return _frameBody;
}

// checkStateOfCharacter: off once three of his hands and feet have let go.
bool Moped::qolCanRemount(CharacterB2D* character)
{
    if (character != _driver || !_frameBody || _vehicleSmashed) {
        return false;
    }
    int lost = (character->qolLostLowerLeg(1) ? 1 : 0) + (character->qolLostLowerLeg(2) ? 1 : 0) +
               (character->qolLostLowerArm(1) ? 1 : 0) + (character->qolLostLowerArm(2) ? 1 : 0);
    return lost <= 2;
}

// ejectCharacter: the frame zero-filtered (qolRestoreFilters), the legs' mask back (attachCharacter
// clears it again); ejectAllCharacters: the frame action (engine pitch, boost meter) removed.
// The driver goes back to the front of _characters (refilterLegs reads _characters[0]).
void Moped::qolRemount(CharacterB2D* character)
{
    qolRestoreFilters(character);
    getLevel()->addToFrameActions(this);
    qolMount(character, [this, character]() {
        attachCharacter(character);
        _characters.insert(_characters.begin(), character);
    });
    qolReplayInjuries(character);
}
