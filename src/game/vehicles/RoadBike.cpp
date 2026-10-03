#include "RoadBike.h"

#include <cmath>
#include <string>

#include "CharacterB2D.h"
#include "LevelB2D.h"
#include "Patch.h"
#include "Session.h"
#include "Settings.h"

USING_NS_CC;

// The constructor and create() are inline in RoadBike.h (as in the original header).

// @0060ea28 (D2), @0060ea6c (D0)
RoadBike::~RoadBike()
{
}

// @006092a0
bool RoadBike::init(Vec2 position, std::string name, int groupID)
{
    _maxTorque = 20.0f;
    _maxSpeed = 40.0f;
    _accelStep = 1.0f;
    _wheelSoundName = "BikeLoop1";
    return Vehicle::init(position, name, groupID);
}

// @00609390
void RoadBike::createSprites()
{
    Node* vehicleBackground = getSession()->getVehicleBackground();

    _frontWheelSprite = Sprite::createWithSpriteFrameName(_name + "_wheel.png");
    vehicleBackground->addChild(_frontWheelSprite);
    _backWheelSprite = Sprite::createWithSpriteFrameName(_name + "_wheel.png");
    vehicleBackground->addChild(_backWheelSprite);
    _frameSprite = Sprite::createWithSpriteFrameName(_name + "_frame.png");
    _frameSprite->setAnchorPoint(_frameAnchor);
    vehicleBackground->addChild(_frameSprite);
    _gearSprite = Sprite::createWithSpriteFrameName(_name + "_gear.png");
    vehicleBackground->addChild(_gearSprite);

    _seatSprite = Sprite::createWithSpriteFrameName(_name + "_childSeat.png");
    _seatSprite->setAnchorPoint(Vec2(1.675f, 0.905f));
    getSession()->getCharacterForeground()->addChild(_seatSprite);
}

// @006098b4
void RoadBike::createFilters()
{
}

// @006098b8
void RoadBike::createBodies()
{
    b2World* world = getWorld();

    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    ValueMap frontWheelShape = bodies.at("frontWheelShape").asValueMap();
    ValueMap backWheelShape = bodies.at("backWheelShape").asValueMap();
    ValueMap gearShape = bodies.at("gearShape").asValueMap();
    ValueMap frame = bodies.at("frame").asValueMap();
    ValueMap seat = bodies.at("seat").asValueMap();
    ValueMap fork = bodies.at("fork").asValueMap();

    b2CircleShape circleShape;
    b2PolygonShape polygonShape;

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.angularDamping = 1.0f;
    bodyDef.allowSleep = false;

    b2FixtureDef fixtureDef;
    fixtureDef.density = 5.0f;
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.3f;
    fixtureDef.filter.categoryBits = 0x201;

    // Wheels and pedal gear: circles.
    Vec2 position = PointFromString(frontWheelShape.at("pos").asString());
    circleShape.m_radius = frontWheelShape.at("radius").asFloat();
    fixtureDef.shape = &circleShape;
    bodyDef.position = b2Vec2(position.x + _origin.x, position.y + _origin.y);
    _frontWheelBody = world->CreateBody(&bodyDef);
    _frontWheelFixture = _frontWheelBody->CreateFixture(&fixtureDef);

    position = PointFromString(backWheelShape.at("pos").asString());
    circleShape.m_radius = backWheelShape.at("radius").asFloat();
    fixtureDef.shape = &circleShape;
    bodyDef.position = b2Vec2(position.x + _origin.x, position.y + _origin.y);
    _backWheelBody = world->CreateBody(&bodyDef);
    _backWheelFixture = _backWheelBody->CreateFixture(&fixtureDef);

    position = PointFromString(gearShape.at("pos").asString());
    circleShape.m_radius = gearShape.at("radius").asFloat();
    fixtureDef.shape = &circleShape;
    bodyDef.position = b2Vec2(position.x + _origin.x, position.y + _origin.y);
    _gearBody = world->CreateBody(&bodyDef);
    _gearBody->CreateFixture(&fixtureDef);

    // Frame: three fixtures built from the plist shapes.
    fixtureDef.density = 2.0f;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    bodyDef.position = b2Vec2(_origin.x, _origin.y);
    _frameBody = world->CreateBody(&bodyDef);
    _frame1Fixture = createFixture(_frameBody, fixtureDef, &frame, false, false);
    fixtureDef.filter = _zeroFilter;
    _frame2Fixture = createFixture(_frameBody, fixtureDef, &fork, false, false);
    fixtureDef.shape = &polygonShape;
    fixtureDef.filter.categoryBits = 0x201;
    fixtureDef.filter.maskBits = 0xffff;
    fixtureDef.filter.groupIndex = 0;
    _frame3Fixture = createFixture(_frameBody, fixtureDef, &seat, false, false);

    addToBeginContact(_frame1Fixture);
    addToBeginContact(_frame2Fixture);
    addToBeginContact(_frame3Fixture);
    addToPostSolve(_frame1Fixture);
    addToPostSolve(_frame2Fixture);
    addToPostSolve(_frame3Fixture);
    addToPostSolve(_frontWheelFixture);
    addToPostSolve(_backWheelFixture);
    addToBeginContact(_backWheelFixture);
    addToBeginContact(_frontWheelFixture);
    addToEndContact(_frontWheelFixture);
    addToEndContact(_backWheelFixture);

    _frontWheelBody->ResetMassData();
    _backWheelBody->ResetMassData();
    _frameBody->ResetMassData();
    _gearBody->ResetMassData();
    _frontWheelBody->SetUserData(_frontWheelSprite);
    _backWheelBody->SetUserData(_backWheelSprite);
    _frameBody->SetUserData(_frameSprite);
    _gearBody->SetUserData(_gearSprite);
    getLevel()->addToPaintBody(_frameBody);
    getLevel()->addToPaintBody(_gearBody);
    getLevel()->addToPaintItem(this);

    // Child seat: its own body with the plist shapes "seat1".."seat3" (the last one a sensor).
    {
        b2BodyDef seatBodyDef;
        seatBodyDef.type = b2_dynamicBody;
        seatBodyDef.angularDamping = 1.0f;
        seatBodyDef.allowSleep = false;
        seatBodyDef.position = b2Vec2(_origin.x, _origin.y);
        _seatBody = world->CreateBody(&seatBodyDef);

        b2PolygonShape seatShape;  // unused (the shapes come from the plist)
        b2FixtureDef seatFixtureDef;
        seatFixtureDef.density = 1.0f;
        seatFixtureDef.friction = 0.3f;
        seatFixtureDef.restitution = 0.1f;
        seatFixtureDef.filter.categoryBits = _defaultFilter.categoryBits;
        seatFixtureDef.filter.maskBits = _defaultFilter.maskBits;
        seatFixtureDef.filter.groupIndex = -2;
        for (unsigned int i = 1; i <= 3; i++) {
            seatFixtureDef.isSensor = (i == 3);
            ValueMap seatPart = bodies.at("seat" + patch::to_string(i)).asValueMap();
            createFixture(_seatBody, seatFixtureDef, &seatPart, false, false);
        }
        _seatBody->ResetMassData();
        _seatBody->SetUserData(_seatSprite);
        getSession()->getLevel()->addToPaintBody(_seatBody);
    }
}

// @0060a768
void RoadBike::lockWheels()
{
    _backWheelJoint->EnableLimit(true);
    _backWheelJoint->SetLimits(0.0f, 0.0f);
}

// @0060a79c
void RoadBike::createJoints()
{
    b2World* world = getWorld();

    b2RevoluteJointDef jointDef;
    jointDef.maxMotorTorque = _maxTorque;
    jointDef.Initialize(_frameBody, _frontWheelBody, _frontWheelBody->GetWorldCenter());
    _frontWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addWheelJoint(_frontWheelJoint, _frontWheelBody);
    jointDef.Initialize(_frameBody, _backWheelBody, _backWheelBody->GetWorldCenter());
    _backWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addWheelJoint(_backWheelJoint, _backWheelBody);
    _frontWheelJoint->SetMaxMotorTorque(_maxTorque);
    _backWheelJoint->SetMaxMotorTorque(_maxTorque);

    // Pedal crank, geared to the back wheel.
    jointDef.Initialize(_frameBody, _gearBody, _gearBody->GetWorldCenter());
    _frameGearJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));

    b2GearJointDef gearJointDef;
    gearJointDef.bodyA = _backWheelBody;
    gearJointDef.bodyB = _gearBody;
    gearJointDef.joint1 = _backWheelJoint;
    gearJointDef.joint2 = _frameGearJoint;
    gearJointDef.ratio = -1.0f;
    _gearJoint = static_cast<b2GearJoint*>(world->CreateJoint(&gearJointDef));

    // Child seat, welded by a locked revolute joint (breaks in checkJoints).
    b2RevoluteJointDef seatJointDef;
    seatJointDef.enableLimit = true;
    ValueMap joints = _bodiesDict.at("joints").asValueMap();
    Vec2 point = PointFromString(joints.at("frameSeatAnchor").asString());
    b2Vec2 anchor(point.x + _origin.x, point.y + _origin.y);
    seatJointDef.Initialize(_frameBody, _seatBody, anchor);
    _seatJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&seatJointDef));
}

// @0060ab3c
void RoadBike::setLimits()
{
}

// @0060ab40
void RoadBike::addContactListeners()
{
}

// @0060ab44
void RoadBike::createDictionaries()
{
    _contactImpulseDict[_frame1Fixture] = 200.0f;
    _contactImpulseDict[_frontWheelFixture] = 200.0f;
    _contactImpulseDict[_backWheelFixture] = 200.0f;

    _contactAddSounds[_backWheelFixture] = "TireHit1";
    _contactAddSounds[_frontWheelFixture] = "TireHit2";
    _contactAddSounds[_frame1Fixture] = "BikeHit3";
    _contactAddSounds[_frame2Fixture] = "BikeHit2";
    _contactAddSounds[_frame3Fixture] = "BikeHit1";
}

// @0060b094
void RoadBike::addDad(CharacterB2D* character)
{
    _dad = character;
    b2World* world = Settings::getInstance()->getCurrentSession()->getWorld();
    Vehicle::addCharacter(character);
    refilterLegs(0);

    // Limit the dad's joints around the riding pose (relative to the current angles).
    float angle = character->getUpperLeg1Body()->GetAngle() - character->getPelvisBody()->GetAngle();
    character->getHipJoint1()->SetLimits(-0.17453292f - angle, 1.91986215f - angle);
    angle = character->getUpperLeg2Body()->GetAngle() - character->getPelvisBody()->GetAngle();
    character->getHipJoint2()->SetLimits(-0.17453292f - angle, 1.91986215f - angle);
    angle = character->getLowerArm1Body()->GetAngle() - character->getUpperArm1Body()->GetAngle();
    character->getElbowJoint1()->SetLimits(-angle, 1.04719758f - angle);
    angle = character->getLowerArm2Body()->GetAngle() - character->getUpperArm2Body()->GetAngle();
    character->getElbowJoint2()->SetLimits(-angle, 1.04719758f - angle);
    angle = character->getHeadBody()->GetAngle() - character->getChestBody()->GetAngle();
    character->getNeckJoint()->SetLimits(-angle, 0.34906584f - angle);

    ValueMap joints = _bodiesDict.at("joints").asValueMap();

    // Hands to the handlebar.
    Vec2 point = PointFromString(joints.at("handleAnchor").asString());
    b2RevoluteJointDef jointDef;
    jointDef.maxMotorTorque = 20.0f;
    b2Vec2 anchor(point.x + _origin.x, point.y + _origin.y);
    jointDef.Initialize(_frameBody, character->getLowerArm1Body(), anchor);
    _vehicleHand1 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addBodyVehicleJoint(character->getLowerArm1Body(), _vehicleHand1);
    jointDef.Initialize(_frameBody, character->getLowerArm2Body(), anchor);
    _vehicleHand2 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addBodyVehicleJoint(character->getLowerArm2Body(), _vehicleHand2);

    // Feet to the pedals.
    point = PointFromString(joints.at("gear1Anchor").asString());
    anchor = b2Vec2(point.x + _origin.x, point.y + _origin.y);
    jointDef.Initialize(_gearBody, character->getLowerLeg1Body(), anchor);
    _vehicleLowerLeg1 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addBodyVehicleJoint(character->getLowerLeg1Body(), _vehicleLowerLeg1);
    point = PointFromString(joints.at("gear2Anchor").asString());
    anchor = b2Vec2(point.x + _origin.x, point.y + _origin.y);
    jointDef.Initialize(_gearBody, character->getLowerLeg2Body(), anchor);
    _vehicleLowerLeg2 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addBodyVehicleJoint(character->getLowerLeg2Body(), _vehicleLowerLeg2);

    // Pelvis to the saddle.
    anchor = character->getPelvisBody()->GetPosition();
    jointDef.Initialize(_frameBody, character->getPelvisBody(), anchor);
    _vehiclePelvis = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addBodyVehicleJoint(character->getPelvisBody(), _vehiclePelvis);
}

// @0060b78c  (maskBits 0: the dad's legs stop colliding; the previous mask is saved first)
void RoadBike::refilterLegs(unsigned int maskBits)
{
    CharacterB2D* dad = _dad;

    b2Filter filter = dad->getUpperLeg1Body()->GetFixtureList()->GetFilterData();
    if (maskBits == 0) {
        _previousMaskBits = filter.maskBits;
    }
    filter.maskBits = maskBits;
    dad->getUpperLeg1Body()->GetFixtureList()->SetFilterData(filter);

    filter = dad->getLowerLeg1Body()->GetFixtureList()->GetFilterData();
    filter.maskBits = maskBits;
    dad->getLowerLeg1Body()->GetFixtureList()->SetFilterData(filter);

    filter = dad->getUpperLeg2Body()->GetFixtureList()->GetFilterData();
    filter.maskBits = maskBits;
    dad->getUpperLeg2Body()->GetFixtureList()->SetFilterData(filter);

    filter = dad->getLowerLeg2Body()->GetFixtureList()->GetFilterData();
    filter.maskBits = maskBits;
    dad->getLowerLeg2Body()->GetFixtureList()->SetFilterData(filter);
}

// @0060b8bc
void RoadBike::addKid(CharacterB2D* character)
{
    _child = character;
    b2World* world = Settings::getInstance()->getCurrentSession()->getWorld();
    Vehicle::addCharacter(character);

    _childGroupIndex = character->getGroupIndex();
    _extraFilter.groupIndex = static_cast<int16>(_childGroupIndex);
    _extraFilter.categoryBits = 4;
    _extraFilter.maskBits = 0xffff;

    b2Filter filter = character->getUpperLeg1Body()->GetFixtureList()->GetFilterData();
    _childPreviousMaskBits = filter.maskBits;
    filter = character->getLowerLeg1Body()->GetFixtureList()->GetFilterData();
    filter.maskBits = 0;
    character->getLowerLeg1Body()->GetFixtureList()->SetFilterData(filter);
    filter = character->getLowerLeg2Body()->GetFixtureList()->GetFilterData();
    filter.maskBits = 0;
    character->getLowerLeg2Body()->GetFixtureList()->SetFilterData(filter);

    b2RevoluteJointDef jointDef;
    jointDef.Initialize(_seatBody, character->getChestBody(), character->getChestBody()->GetPosition());
    _seatChest = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addBodyVehicleJoint(character->getChestBody(), _seatChest);
    jointDef.Initialize(_seatBody, character->getPelvisBody(), character->getPelvisBody()->GetPosition());
    _seatPelvis = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addBodyVehicleJoint(character->getPelvisBody(), _seatPelvis);
    jointDef.Initialize(_seatBody, character->getUpperLeg1Body(), character->getUpperLeg1Body()->GetPosition());
    _seatUpperLeg1 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addBodyVehicleJoint(character->getUpperLeg1Body(), _seatUpperLeg1);
    jointDef.Initialize(_seatBody, character->getUpperLeg2Body(), character->getUpperLeg2Body()->GetPosition());
    _seatUpperLeg2 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addBodyVehicleJoint(character->getUpperLeg2Body(), _seatUpperLeg2);

    Node* upperArmSprite = static_cast<Node*>(character->getUpperArm1Body()->GetUserData());
    _seatSprite->setLocalZOrder(upperArmSprite->getLocalZOrder() - 1);
}

// @0060bba4  (the dad falls off once both feet are off the pedals)
void RoadBike::checkStateOfCharacter(CharacterB2D* character)
{
    if (_dad != character) {
        return;
    }
    b2Joint* leg1Joint = _bodyVehicleJointDict[character->getLowerLeg1Body()];
    b2Joint* leg2Joint = _bodyVehicleJointDict[character->getLowerLeg2Body()];
    if (leg2Joint == nullptr && leg1Joint == nullptr) {
        ejectCharacter(character);
    }
}

// @0060bd58
bool RoadBike::ejectCharacter(CharacterB2D* character)
{
    bool ejected = Vehicle::ejectCharacter(character);
    if (!ejected) {
        return ejected;
    }

    if (_dad == character) {
        forwardBackButtonsNull();
        leanButtonsNull();
        special1ButtonNull();
        _frontWheelBody->GetFixtureList()->SetFilterData(_zeroFilter);
        _backWheelBody->GetFixtureList()->SetFilterData(_zeroFilter);
        for (b2Fixture* fixture = _frameBody->GetFixtureList(); fixture; fixture = fixture->GetNext()) {
            fixture->SetFilterData(_zeroFilter);
        }
        refilterLegs(_previousMaskBits);
    } else if (_child == character) {
        _childEjected = true;

        b2Filter filter = character->getUpperLeg1Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = static_cast<uint16>(_childPreviousMaskBits);
        character->getUpperLeg1Body()->GetFixtureList()->SetFilterData(filter);
        filter = character->getLowerLeg1Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = static_cast<uint16>(_childPreviousMaskBits);
        character->getLowerLeg1Body()->GetFixtureList()->SetFilterData(filter);
        filter = character->getUpperLeg2Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = static_cast<uint16>(_childPreviousMaskBits);
        character->getUpperLeg2Body()->GetFixtureList()->SetFilterData(filter);
        filter = character->getLowerLeg2Body()->GetFixtureList()->GetFilterData();
        filter.maskBits = static_cast<uint16>(_childPreviousMaskBits);
        character->getLowerLeg2Body()->GetFixtureList()->SetFilterData(filter);

        if (!_seatDetached) {
            if (character->getHeadBody()) {
                refilterShit(character->getHeadBody()->GetFixtureList());
            }
            if (character->getChestBody()) {
                refilterShit(character->getChestBody()->GetFixtureList());
            }
            refilterShit(character->getUpperArm1Body()->GetFixtureList());
            refilterShit(character->getUpperArm2Body()->GetFixtureList());
            refilterShit(character->getLowerArm1Body()->GetFixtureList());
            refilterShit(character->getLowerArm2Body()->GetFixtureList());
            refilterShit(character->getUpperLeg1Body()->GetFixtureList());
            refilterShit(character->getUpperLeg2Body()->GetFixtureList());
            refilterShit(character->getLowerLeg1Body()->GetFixtureList());
            refilterShit(character->getLowerLeg2Body()->GetFixtureList());
            for (b2Fixture* fixture = _seatBody->GetFixtureList(); fixture; fixture = fixture->GetNext()) {
                fixture->SetFilterData(_zeroFilter);
            }
        }
    }
    return ejected;
}

// @0060c0f0  (child fixtures that still use the default mask get the seat filter)
void RoadBike::refilterShit(b2Fixture* fixture)
{
    if (fixture->GetFilterData().maskBits == _defaultFilter.maskBits) {
        fixture->SetFilterData(_extraFilter);
    }
}

// @0060c114
void RoadBike::leanBackPose()
{
    CharacterB2D* dad = _dad;
    dad->setJoint(dad->getNeckJoint(), 0.0f, 2.0f, 20.0f);
    if (dad->getElbowJoint1() && dad->getShoulderJoint1() && !dad->getUpperArm3Body()) {
        dad->setJoint(dad->getElbowJoint1(), -1.04f, 15.0f, 20.0f);
    }
    if (dad->getElbowJoint2() && dad->getShoulderJoint2() && !dad->getUpperArm4Body()) {
        dad->setJoint(dad->getElbowJoint2(), -1.04f, 15.0f, 20.0f);
    }
}

// @0060c1f0
void RoadBike::leanForwardPose()
{
    CharacterB2D* dad = _dad;
    dad->setJoint(dad->getNeckJoint(), -1.0f, 1.0f, 20.0f);
    if (dad->getElbowJoint1() && dad->getShoulderJoint1() && !dad->getUpperArm3Body()) {
        dad->setJoint(dad->getElbowJoint1(), 0.0f, 15.0f, 20.0f);
    }
    if (dad->getElbowJoint2() && dad->getShoulderJoint2() && !dad->getUpperArm4Body()) {
        dad->setJoint(dad->getElbowJoint2(), 0.0f, 15.0f, 20.0f);
    }
}

// @0060c2bc
void RoadBike::noLeanBackPose()
{
}

// @0060c2c0
void RoadBike::noLeanForwardPose()
{
}

// @0060c2c4
void RoadBike::actions()
{
    Vehicle::actions();
}

// @0060c2c8  (the wheels are not in the level's paint list; their sprites are the body user data)
void RoadBike::paint()
{
    Sprite* sprite = static_cast<Sprite*>(_frontWheelBody->GetUserData());
    b2Vec2 center = _frontWheelBody->GetWorldCenter();
    sprite->setPosition(Vec2(center.x * getPtm(), center.y * getPtm()));
    sprite->setRotation(-CC_RADIANS_TO_DEGREES(_frontWheelBody->GetAngle()) - _frontWheelAngOffset);

    sprite = static_cast<Sprite*>(_backWheelBody->GetUserData());
    center = _backWheelBody->GetWorldCenter();
    sprite->setPosition(Vec2(center.x * getPtm(), center.y * getPtm()));
    sprite->setRotation(-CC_RADIANS_TO_DEGREES(_backWheelBody->GetAngle()) - _backWheelAngOffset);
}

// @0060c3f4
void RoadBike::checkJoints()
{
    if (!_seatDetached && checkRevJoint(_seatJoint, _childSeatBreakLimit)) {
        detachSeat();
    }
}

// @0060c440
void RoadBike::detachSeat()
{
    if (_seatDetached) {
        return;
    }
    _seatDetached = true;
    getWorld()->DestroyJoint(_seatJoint);

    if (_childEjected) {
        for (b2Fixture* fixture = _seatBody->GetFixtureList(); fixture; fixture = fixture->GetNext()) {
            fixture->SetSensor(false);
            fixture->Refilter();
        }
        return;
    }
    if (_child == nullptr) {
        return;
    }

    if (_child->getHeadBody()) {
        refilterShit(_child->getHeadBody()->GetFixtureList());
    }
    if (_child->getPelvisBody()) {
        refilterShit(_child->getPelvisBody()->GetFixtureList());
    }
    if (_child->getChestBody()) {
        refilterShit(_child->getChestBody()->GetFixtureList());
    }
    refilterShit(_child->getUpperArm1Body()->GetFixtureList());
    refilterShit(_child->getUpperArm2Body()->GetFixtureList());
    refilterShit(_child->getLowerArm1Body()->GetFixtureList());
    refilterShit(_child->getLowerArm2Body()->GetFixtureList());
    refilterShit(_child->getUpperLeg1Body()->GetFixtureList());
    refilterShit(_child->getUpperLeg2Body()->GetFixtureList());
    refilterShit(_child->getLowerLeg1Body()->GetFixtureList());
    refilterShit(_child->getLowerLeg2Body()->GetFixtureList());

    b2Filter filter = _child->getLowerLeg1Body()->GetFixtureList()->GetFilterData();
    filter.maskBits = static_cast<uint16>(_childPreviousMaskBits);
    _child->getLowerLeg1Body()->GetFixtureList()->SetFilterData(filter);
    filter = _child->getLowerLeg2Body()->GetFixtureList()->GetFilterData();
    filter.maskBits = static_cast<uint16>(_childPreviousMaskBits);
    _child->getLowerLeg2Body()->GetFixtureList()->SetFilterData(filter);

    for (b2Fixture* fixture = _seatBody->GetFixtureList(); fixture; fixture = fixture->GetNext()) {
        fixture->SetFilterData(_extraFilter);
        fixture->Refilter();
        fixture->SetSensor(false);
    }
}

// @0060c73c  (the frame breaks into seat post, fork and broken frame; the impulse is unused)
void RoadBike::frameSmash(float impulse)
{
    stopSoundsForBody(_frameBody);
    detachSeat();
    _vehicleSmashed = true;
    removePostSolve(_frame1Fixture);
    removePostSolve(_frame2Fixture);
    removePostSolve(_frame3Fixture);
    removeBeginContact(_frame1Fixture);
    removeBeginContact(_frame2Fixture);
    removeBeginContact(_frame3Fixture);
    removeBeginContact(_frontWheelFixture);
    removeBeginContact(_backWheelFixture);
    removeEndContact(_frontWheelFixture);
    removeEndContact(_backWheelFixture);
    ejectAllCharacters();
    getLevel()->removeFromPaintBody(_frameBody);

    b2Vec2 linearVelocity = _frameBody->GetLinearVelocity();
    float angularVelocity = _frameBody->GetAngularVelocity();
    b2Vec2 position = _frameBody->GetPosition();
    float angle = _frameBody->GetAngle();
    b2Fixture* fixture = _frameBody->GetFixtureList();
    float friction = fixture->GetFriction();
    float restitution = fixture->GetRestitution();
    float density = fixture->GetDensity();
    do {
        fixtureWillBeDestroyed(fixture);
        fixture = fixture->GetNext();
    } while (fixture);

    b2World* world = Settings::getInstance()->getCurrentSession()->getWorld();
    world->DestroyJoint(_gearJoint);
    world->DestroyJoint(_frontWheelJoint);
    world->DestroyJoint(_backWheelJoint);
    world->DestroyBody(_frameBody);
    _frameBody = nullptr;
    _backWheelJoint = nullptr;
    _frontWheelJoint = nullptr;
    _frameGearJoint = nullptr;
    _gearJoint = nullptr;
    _frameSprite->removeFromParentAndCleanup(false);

    Node* vehicleBackground = getSession()->getVehicleBackground();
    _frameSprite = Sprite::createWithSpriteFrameName(_name + "_brokenFrame.png");
    Sprite* seatSprite = Sprite::createWithSpriteFrameName(_name + "_seat.png");
    Sprite* forkSprite = Sprite::createWithSpriteFrameName(_name + "_fork.png");
    // (the anchor points are set twice in the original)
    _frameSprite->setAnchorPoint(_brokenFrameAnchor);
    seatSprite->setAnchorPoint(_seatAnchor);
    forkSprite->setAnchorPoint(_forkAnchor);
    _frameSprite->setAnchorPoint(_brokenFrameAnchor);
    seatSprite->setAnchorPoint(_seatAnchor);
    forkSprite->setAnchorPoint(_forkAnchor);
    vehicleBackground->addChild(_frameSprite);
    vehicleBackground->addChild(seatSprite);
    vehicleBackground->addChild(forkSprite);

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.angle = angle;
    bodyDef.allowSleep = false;

    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    ValueMap broken = bodies.at("broken").asValueMap();
    ValueMap seat = bodies.at("seat").asValueMap();
    ValueMap fork = bodies.at("fork").asValueMap();

    b2FixtureDef fixtureDef;
    fixtureDef.friction = friction;
    fixtureDef.restitution = restitution;
    fixtureDef.density = density;
    fixtureDef.filter = _zeroFilter;

    b2Body* body = world->CreateBody(&bodyDef);
    createFixture(body, fixtureDef, &seat, false, false);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);
    body->SetUserData(seatSprite);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);

    body = world->CreateBody(&bodyDef);
    createFixture(body, fixtureDef, &fork, false, false);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);
    body->SetUserData(forkSprite);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);

    body = world->CreateBody(&bodyDef);
    createFixture(body, fixtureDef, &broken, false, false);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);
    body->SetUserData(_frameSprite);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);

    _gearBody->GetFixtureList()->SetFilterData(_zeroFilter);
    createBodySound("BikeSmash1", body, 1.0f, false);
    getLevel()->removeFromActions(this);
}

// @0060d2e0  (the tyre bursts: the circle becomes a box; only the normal is used)
void RoadBike::frontWheelSmash(float impulse, b2Vec2 normal)
{
    _contactResultBufferDict.erase(_frontWheelFixture);
    removePostSolve(_frontWheelFixture);

    Node* parent = _frontWheelSprite->getParent();
    Sprite* brokenSprite = Sprite::createWithSpriteFrameName(_name + "_brokenWheel.png");
    int zOrder = _frontWheelSprite->getLocalZOrder();
    parent->removeChild(_frontWheelSprite, false);
    parent->addChild(brokenSprite, zOrder - 1);
    _frontWheelBody->SetUserData(brokenSprite);

    b2PolygonShape shape;
    float radius = _frontWheelFixture->GetShape()->m_radius;
    float angle = atan2f(normal.y, normal.x) - _frontWheelBody->GetAngle();
    brokenSprite->setRotation(angle * 180.0f / M_PI);
    shape.SetAsBox(radius * 0.46f, radius * 0.92f, b2Vec2(0.0f, 0.0f), angle);
    _frontWheelAngOffset = CC_RADIANS_TO_DEGREES(angle);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.density = _frontWheelFixture->GetDensity();
    fixtureDef.friction = _frontWheelFixture->GetFriction();
    fixtureDef.restitution = _frontWheelFixture->GetRestitution();
    if (!_ejected) {
        fixtureDef.filter = _zeroFilter;
    } else {
        fixtureDef.filter = _defaultFilter;
    }
    fixtureWillBeDestroyed(_frontWheelFixture);
    _frontWheelBody->DestroyFixture(_frontWheelBody->GetFixtureList());
    _frontWheelFixture = _frontWheelBody->CreateFixture(&fixtureDef);
    _frontWheelBody->ResetMassData();
    createBodySound("BikeTireSmash1", _frontWheelBody, 1.0f, false);
}

// @0060d73c
void RoadBike::backWheelSmash(float impulse, b2Vec2 normal)
{
    _contactResultBufferDict.erase(_backWheelFixture);
    removePostSolve(_backWheelFixture);

    Node* parent = _backWheelSprite->getParent();
    Sprite* brokenSprite = Sprite::createWithSpriteFrameName(_name + "_brokenWheel.png");
    int zOrder = _backWheelSprite->getLocalZOrder();
    parent->removeChild(_backWheelSprite, false);
    parent->addChild(brokenSprite, zOrder - 1);
    _backWheelBody->SetUserData(brokenSprite);

    b2PolygonShape shape;
    float radius = _backWheelFixture->GetShape()->m_radius;
    // The original measures the angle against the FRONT wheel body here (kept).
    float angle = atan2f(normal.y, normal.x) - _frontWheelBody->GetAngle();
    brokenSprite->setRotation(angle * 180.0f / M_PI);
    shape.SetAsBox(radius * 0.46f, radius * 0.92f, b2Vec2(0.0f, 0.0f), angle);
    _backWheelAngOffset = CC_RADIANS_TO_DEGREES(angle);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.density = _backWheelFixture->GetDensity();
    fixtureDef.friction = _backWheelFixture->GetFriction();
    fixtureDef.restitution = _backWheelFixture->GetRestitution();
    if (!_ejected) {
        fixtureDef.filter = _zeroFilter;
    } else {
        fixtureDef.filter = _defaultFilter;
    }
    fixtureWillBeDestroyed(_backWheelFixture);
    _backWheelBody->DestroyFixture(_backWheelBody->GetFixtureList());
    _backWheelFixture = _backWheelBody->CreateFixture(&fixtureDef);
    _backWheelBody->ResetMassData();
    createBodySound("BikeTireSmash1", _backWheelBody, 1.0f, false);
}

// @0060db9c
void RoadBike::forwardButtonPressed()
{
    if (_dad->getDead()) {
        return;
    }
    if (_wheelContacts == 0) {
        _frontWheelJoint->EnableMotor(false);
        _backWheelJoint->EnableMotor(false);
    } else if (!_backWheelJoint->IsMotorEnabled()) {
        _backWheelJoint->EnableMotor(true);
        _frontWheelJoint->EnableMotor(true);
    }
    float speed = _backWheelJoint->GetJointSpeed();
    float motorSpeed = speed > 0.0f ? 0.0f : speed - (speed <= -_maxSpeed ? 0.0f : _accelStep);
    _backWheelJoint->SetMotorSpeed(motorSpeed);
    _frontWheelJoint->SetMotorSpeed(motorSpeed);
}

// @0060dc60
void RoadBike::backButtonPressed()
{
    if (_dad->getDead()) {
        return;
    }
    if (_wheelContacts == 0) {
        _frontWheelJoint->EnableMotor(false);
        _backWheelJoint->EnableMotor(false);
    } else if (!_backWheelJoint->IsMotorEnabled()) {
        _backWheelJoint->EnableMotor(true);
        _frontWheelJoint->EnableMotor(true);
    }
    float speed = _backWheelJoint->GetJointSpeed();
    float motorSpeed = speed < 0.0f ? 0.0f : speed - (speed >= _maxSpeed ? 0.0f : -_accelStep);
    _backWheelJoint->SetMotorSpeed(motorSpeed);
    _frontWheelJoint->SetMotorSpeed(motorSpeed);
}

// @0060dd20
void RoadBike::forwardBackButtonsNull()
{
    for (int i = 0; i < _wheelJoints.size(); i++) {
        b2RevoluteJoint* joint = _wheelJoints[i];
        if (joint->IsMotorEnabled()) {
            joint->EnableMotor(false);
        }
    }
    if (_currentPose == VehiclePoseForward || _currentPose == VehiclePoseBack) {
        setCurrentPose(VehiclePoseNone);
    }
}

// @0060ddbc  (brake)
void RoadBike::special1ButtonPressed()
{
    if (!_backWheelJoint->IsMotorEnabled()) {
        _backWheelJoint->EnableMotor(true);
        _frontWheelJoint->EnableMotor(true);
    }
    _backWheelJoint->SetMotorSpeed(0.0f);
    _frontWheelJoint->SetMotorSpeed(0.0f);
}

// @0060de10
void RoadBike::leanBackButtonPressed()
{
    setCurrentPose(VehiclePoseLeanBack);

    float angularVelocity = _frameBody->GetAngularVelocity();
    double angle = _frameBody->GetAngle() + M_PI;
    float factor = b2Min(b2Max((double)((angularVelocity - _maxSpinAV) / -_maxSpinAV), 0.0), 1.0);
    double magnitude = _impulseMagnitudeMax * s_timeStepOverFlashTimeStep;
    b2Vec2 impulse((float)(sin(angle) * magnitude * factor), -(float)(cos(angle) * magnitude * factor));
    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x + _impulseOffset, localCenter.y)), true);
}

// @0060df70
void RoadBike::leanForwardButtonPressed()
{
    setCurrentPose(VehiclePoseLeanForward);

    float angularVelocity = _frameBody->GetAngularVelocity();
    double angle = _frameBody->GetAngle() + M_PI;
    float factor = b2Min(b2Max((angularVelocity + _maxSpinAV) / _maxSpinAV, 0.0f), 1.0f);
    double magnitude = _impulseMagnitudeMax * s_timeStepOverFlashTimeStep;
    b2Vec2 impulse((float)(sin(angle) * magnitude * factor), -(float)(cos(angle) * magnitude * factor));
    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x - _impulseOffset, localCenter.y)), true);
}

// @0060e0c0
void RoadBike::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (fixture == _frontWheelFixture || fixture == _backWheelFixture) {
        if (otherFixture->IsSensor()) {
            return;
        }
        _wheelContacts++;
    }
    contactSoundHandler(fixture, otherFixture, contact, nullptr);
}

// @0060e0f8
void RoadBike::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if ((fixture == _frontWheelFixture || fixture == _backWheelFixture) && !otherFixture->IsSensor()) {
        _wheelContacts--;
    }
}

// @0060e124  (frame hits are all filed under _frame1Fixture; wheels also keep the contact normal)
void RoadBike::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                         const b2ContactImpulse* impulse)
{
    float normalImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        normalImpulse = b2Max(normalImpulse, impulse->normalImpulses[1]);
    }

    if (fixture == _frame1Fixture || fixture == _frame2Fixture || fixture == _frame3Fixture) {
        if (normalImpulse > _contactImpulseDict[_frame1Fixture]) {
            _contactResultBufferDict[_frame1Fixture].impulse = normalImpulse;
        }
    } else if (fixture == _frontWheelFixture || fixture == _backWheelFixture) {
        if (normalImpulse > _contactImpulseDict[fixture]) {
            _contactResultBufferDict[fixture].impulse = normalImpulse;
            _contactResultBufferDict[fixture].normal = contact->GetManifold()->localNormal;
        }
    }
}

// @0060e4cc
void RoadBike::handleContactResults()
{
    if (_contactResultBufferDict[_frame1Fixture].impulse != 0.0f) {
        frameSmash(_contactResultBufferDict[_frame1Fixture].impulse);
    }
    if (_contactResultBufferDict[_frontWheelFixture].impulse != 0.0f) {
        frontWheelSmash(_contactResultBufferDict[_frontWheelFixture].impulse,
                        _contactResultBufferDict[_frontWheelFixture].normal);
    }
    if (_contactResultBufferDict[_backWheelFixture].impulse != 0.0f) {
        backWheelSmash(_contactResultBufferDict[_backWheelFixture].impulse,
                       _contactResultBufferDict[_backWheelFixture].normal);
    }
    _contactResultBufferDict.clear();
}

// @0060ea24  (debug: drops the child seat)
void RoadBike::debugFunction(int value)
{
    detachSeat();
}
