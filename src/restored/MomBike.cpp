// RESTORED (PC addition): Irresponsible Mom's bicycle, the daughter's trailer bike and the son's
// basket, ported from the browser game's BicycleGuy.as / IrresponsibleMom.as / IMDaughter.as /
// IMSon.as (Flash v1.87) onto the mobile Vehicle framework. See MomBike.h.

#include "MomBike.h"

#include <algorithm>
#include <cmath>

#include "CharacterB2D.h"
#include "FlashParticles.h"
#include "LevelB2D.h"
#include "Patch.h"
#include "Session.h"
#include "Settings.h"
#include "platform/compat/Box2DFloat.h"

USING_NS_CC;

namespace {

const float kDeg = 0.017453292f;

// Flash joint limits (degrees, y down) relative to the current pose -> mobile (y up): the range
// flips sign and swaps ends, as in RoadBike::addDad.
void limitJoint(b2RevoluteJoint* joint, b2Body* a, b2Body* b, float lowerDeg, float upperDeg)
{
    if (!joint) {
        return;
    }
    float angle = b->GetAngle() - a->GetAngle();
    joint->SetLimits(-upperDeg * kDeg - angle, -lowerDeg * kDeg - angle);
}

}  // namespace

MomBike* MomBike::create(Vec2 position, std::string name, int groupID)
{
    MomBike* bike = new (std::nothrow) MomBike();
    if (bike && bike->init(position, name, groupID)) {
        bike->autorelease();
        return bike;
    }
    delete bike;
    return nullptr;
}

MomBike::MomBike()
    : _frameSmashLimit(100.0f),
      _wheelSmashLimit(200.0f),
      _impulseMagnitude(3.0f),
      _impulseOffset(1.0f),
      _maxSpinAV(5.0f),
      _daughterMaxSpeed(55.4f),
      _daughterAccelStep(1.385f),
      _daughterFrameSmashLimit(50.0f),
      _daughterDetachLimit(800.0f),
      _basketSmashLimit(3.0f),
      _basketDetachLimit(200.0f),
      _sonEjectImpulse(0.75f),
      _mom(nullptr),
      _daughter(nullptr),
      _son(nullptr),
      _momEjected(false),
      _daughterEjected(false),
      _sonEjected(false),
      _frameSmashed(false),
      _daughterDetached(false),
      _daughterSmashed(false),
      _basketDetached(false),
      _basketSmashed(false),
      _frameBody(nullptr),
      _frontWheelBody(nullptr),
      _backWheelBody(nullptr),
      _gearBody(nullptr),
      _seatFixture(nullptr),
      _frameFixture(nullptr),
      _forkFixture(nullptr),
      _frontWheelJoint(nullptr),
      _backWheelJoint(nullptr),
      _frameGearJoint(nullptr),
      _gearJoint(nullptr),
      _dFrameBody(nullptr),
      _dWheelBody(nullptr),
      _dGearBody(nullptr),
      _dSeatFixture(nullptr),
      _dFrameFixture(nullptr),
      _dHandleFixture(nullptr),
      _dMidFixture(nullptr),
      _dEndFixture(nullptr),
      _dWheelFixture(nullptr),
      _dWheelJoint(nullptr),
      _dFrameGearJoint(nullptr),
      _dGearJoint(nullptr),
      _dConnectingJoint(nullptr),
      _basketBody(nullptr),
      _basketJoint(nullptr),
      _frameSprite(nullptr),
      _gearSprite(nullptr),
      _frontWheelSprite(nullptr),
      _backWheelSprite(nullptr),
      _dFrameSprite(nullptr),
      _dWheelSprite(nullptr),
      _dGearSprite(nullptr),
      _basketSprite(nullptr)
{
    _frontWheelFixture = nullptr;
    _backWheelFixture = nullptr;
}

MomBike::~MomBike()
{
}

bool MomBike::gameplay()
{
    return getSession()->getMode() == SessionModeGameplay;
}

bool MomBike::init(Vec2 position, std::string name, int groupID)
{
    // As RoadBike (the mobile port's version of the same BicycleGuy): Flash wheelMaxSpeed 20 -> 40,
    // accelStep 1, maxTorque 20.
    _maxTorque = 20.0f;
    _maxSpeed = 40.0f;
    _accelStep = 1.0f;
    _wheelSoundName = "BikeLoop1";
    bool ok = Vehicle::init(position, name, groupID);
    if (ok) {
        getLevel()->addToPaintItem(this);
    }
    return ok;
}

// ---- helpers ---------------------------------------------------------------------------------

ValueMap MomBike::shape(const std::string& name)
{
    return _bodiesDict.at("bodies").asValueMap().at(name).asValueMap();
}

b2Vec2 MomBike::point(const std::string& name)
{
    Vec2 p = PointFromString(_bodiesDict.at("joints").asValueMap().at(name).asString()) + _origin;
    return b2Vec2(p.x, p.y);
}

// A polygon from the guide points <prefix>1..<prefix><count>, in the body's local frame (the
// bodies sit on the guide origin, as Flash's on (_startX, _startY)).
b2Fixture* MomBike::addPolygon(b2Body* body, b2FixtureDef def, const std::string& prefix, int count)
{
    b2Vec2 vertices[b2_maxPolygonVertices];
    for (int i = 0; i < count; i++) {
        vertices[i] = body->GetLocalPoint(point(prefix + patch::to_string(i + 1)));
    }
    b2PolygonShape polygon;
    polygon.Set(vertices, count);
    def.shape = &polygon;
    return body->CreateFixture(&def);
}

b2Body* MomBike::addCircleBody(const std::string& name, b2FixtureDef def)
{
    ValueMap data = shape(name);
    Vec2 p = PointFromString(data.at("pos").asString()) + _origin;
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.angularDamping = 1.0f;  // as RoadBike
    bodyDef.allowSleep = false;
    bodyDef.position.Set(p.x, p.y);
    b2Body* body = getWorld()->CreateBody(&bodyDef);
    b2CircleShape circle;
    circle.m_radius = data.at("radius").asFloat();
    def.shape = &circle;
    body->CreateFixture(&def);
    return body;
}

Sprite* MomBike::sprite(const std::string& frame, Node* parent, int z)
{
    Sprite* s = Sprite::createWithSpriteFrameName(_name + "_" + frame + ".png");
    parent->addChild(s, z);
    return s;
}

void MomBike::paintBody(b2Body* body, Sprite* sprite, bool atCentre)
{
    _painted.push_back({body, sprite, atCentre, 0.0f});
}

void MomBike::unpaint(b2Body* body)
{
    _painted.erase(std::remove_if(_painted.begin(), _painted.end(),
                                  [body](const Painted& p) { return p.body == body; }),
                   _painted.end());
}

void MomBike::destroyJoint(b2Joint*& joint)
{
    if (joint) {
        getWorld()->DestroyJoint(joint);
        joint = nullptr;
    }
}

// Flash IMDaughter/IMSon.refilterShit: a kid's part that still uses the default mask now hits
// everything but the kid itself (extraFilter); the daughter's also stops being a sensor.
void MomBike::refilterKid(CharacterB2D* kid, b2Fixture* fixture, bool clearSensor)
{
    if (!fixture) {
        return;
    }
    if (clearSensor) {
        fixture->SetSensor(false);
    }
    if (fixture->GetFilterData().maskBits == _defaultFilter.maskBits) {
        fixture->SetFilterData(kid == _daughter ? _daughterExtraFilter : _sonExtraFilter);
    }
    fixture->Refilter();
}

void MomBike::refilterKidParts(CharacterB2D* kid, bool clearSensor, bool upperOnly)
{
    std::vector<b2Body*> bodies = {kid->getHeadBody(),      kid->getPelvisBody(),
                                   kid->getChestBody(),     kid->getUpperArm1Body(),
                                   kid->getUpperArm2Body(), kid->getLowerArm1Body(),
                                   kid->getLowerArm2Body()};
    if (!upperOnly) {
        for (b2Body* b : {kid->getUpperLeg1Body(), kid->getUpperLeg2Body(), kid->getLowerLeg1Body(),
                          kid->getLowerLeg2Body()}) {
            bodies.push_back(b);
        }
    }
    for (b2Body* body : bodies) {
        if (body) {
            refilterKid(kid, body->GetFixtureList(), clearSensor);
        }
    }
}

// ---- creation --------------------------------------------------------------------------------

void MomBike::createSprites()
{
    // Flash adds the bikes just below each rider's chest, the basket on top of everything.
    Node* background = getSession()->getVehicleBackground();
    _dWheelSprite = sprite("daughterWheel", background, -6);
    _dGearSprite = sprite("daughterGear", background, -5);
    _dFrameSprite = sprite("daughterFrame", background, -4);
    _backWheelSprite = sprite("backWheel", background, 0);
    _frontWheelSprite = sprite("frontWheel", background, 1);
    _gearSprite = sprite("gear", background, 2);
    _frameSprite = sprite("frame", background, 3);
    _basketSprite = sprite("basket", getSession()->getVehicleForeground(), 0);
}

void MomBike::createBodies()
{
    b2World* world = getWorld();

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.angularDamping = 1.0f;
    bodyDef.allowSleep = false;
    bodyDef.position.Set(_origin.x, _origin.y);

    // Flash BicycleGuy: polygons density 2, friction 0.3, restitution 0.1; circles density 5,
    // friction 1, restitution 0.3; category 513 (0x201) unless zeroFilter.
    b2FixtureDef poly;
    poly.density = 2.0f;
    poly.friction = 0.3f;
    poly.restitution = 0.1f;
    poly.filter.categoryBits = 0x201;
    b2FixtureDef zeroPoly = poly;
    zeroPoly.filter = _zeroFilter;
    b2FixtureDef circle;
    circle.density = 5.0f;
    circle.friction = 1.0f;
    circle.restitution = 0.3f;
    circle.filter.categoryBits = 0x201;

    // -- the mom's bicycle
    _frameBody = world->CreateBody(&bodyDef);
    _seatFixture = addPolygon(_frameBody, poly, "seatVert", 3);
    _frameFixture = addPolygon(_frameBody, zeroPoly, "frameVert", 4);
    _forkFixture = addPolygon(_frameBody, zeroPoly, "forkVert", 3);
    _frameBody->ResetMassData();
    _backWheelBody = addCircleBody("backWheelShape", circle);
    _backWheelFixture = _backWheelBody->GetFixtureList();
    _frontWheelBody = addCircleBody("frontWheelShape", circle);
    _frontWheelFixture = _frontWheelBody->GetFixtureList();
    _gearBody = addCircleBody("gearShape", circle);

    // -- the daughter's trailer bike (IMDaughter.createBodies; her filters are set in addDaughter)
    _dFrameBody = world->CreateBody(&bodyDef);
    _dSeatFixture = addPolygon(_dFrameBody, poly, "daughter_seatVert", 3);
    _dFrameFixture = addPolygon(_dFrameBody, zeroPoly, "daughter_frameVert", 3);
    _dHandleFixture = addPolygon(_dFrameBody, zeroPoly, "daughter_handleVert", 3);
    _dMidFixture = addPolygon(_dFrameBody, zeroPoly, "daughter_midVert", 4);
    b2FixtureDef endDef = zeroPoly;
    endDef.isSensor = true;
    _dEndFixture = addPolygon(_dFrameBody, endDef, "daughter_endVert", 4);
    _dFrameBody->ResetMassData();
    _dWheelBody = addCircleBody("daughter_wheelShape", circle);
    _dWheelFixture = _dWheelBody->GetFixtureList();
    _dGearBody = addCircleBody("daughter_gearShape", circle);

    // -- the son's basket (IMSon.createBodies: density 1, the mom's default filter)
    b2FixtureDef basket;
    basket.density = 1.0f;
    basket.friction = 0.3f;
    basket.restitution = 0.1f;
    basket.filter = _defaultFilter;
    _basketBody = world->CreateBody(&bodyDef);
    _basketFixtures.push_back(addPolygon(_basketBody, basket, "son_crateRight", 4));
    _basketFixtures.push_back(addPolygon(_basketBody, basket, "son_crateBottom", 4));
    _basketFixtures.push_back(addPolygon(_basketBody, basket, "son_crateLeft", 4));
    _basketBody->ResetMassData();

    for (b2Fixture* f : {_seatFixture, _frameFixture, _forkFixture, _dSeatFixture, _dFrameFixture,
                         _dHandleFixture, _dMidFixture, _dEndFixture}) {
        addToPostSolve(f);
    }
    for (b2Fixture* f : {_seatFixture, _frameFixture, _forkFixture, _dFrameFixture, _dHandleFixture,
                         _dMidFixture, _dWheelFixture}) {
        addToBeginContact(f);
    }
    addToPostSolve(_frontWheelFixture);
    addToPostSolve(_backWheelFixture);
    for (b2Fixture* f : _basketFixtures) {
        addToPostSolve(f);
        addToBeginContact(f);
    }

    paintBody(_frameBody, _frameSprite, true);
    paintBody(_gearBody, _gearSprite, true);
    paintBody(_frontWheelBody, _frontWheelSprite, true);
    paintBody(_backWheelBody, _backWheelSprite, true);
    paintBody(_dFrameBody, _dFrameSprite, false);
    paintBody(_dWheelBody, _dWheelSprite, true);
    paintBody(_dGearBody, _dGearSprite, true);
    paintBody(_basketBody, _basketSprite, false);
}

void MomBike::createJoints()
{
    b2World* world = getWorld();

    b2RevoluteJointDef def;
    def.maxMotorTorque = _maxTorque;
    def.Initialize(_frameBody, _backWheelBody, _backWheelBody->GetPosition());
    _backWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&def));
    def.Initialize(_frameBody, _frontWheelBody, _frontWheelBody->GetPosition());
    _frontWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&def));
    addWheelJoint(_backWheelJoint, _backWheelBody);
    addWheelJoint(_frontWheelJoint, _frontWheelBody);
    _wheelJointSpeedDict[_frontWheelJoint] = 1.0f;  // both wheels get the same speed (Flash)
    def.Initialize(_frameBody, _gearBody, _gearBody->GetPosition());
    _frameGearJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&def));

    b2GearJointDef gear;
    gear.bodyA = _backWheelBody;
    gear.bodyB = _gearBody;
    gear.joint1 = _backWheelJoint;
    gear.joint2 = _frameGearJoint;
    gear.ratio = -1.0f;
    _gearJoint = static_cast<b2GearJoint*>(world->CreateJoint(&gear));

    // Daughter's trailer: her wheel (motor torque 20), the gear and the hitch on the mom's frame
    // (limited to +-35 degrees).
    def.Initialize(_dFrameBody, _dWheelBody, _dWheelBody->GetPosition());
    _dWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&def));
    def.Initialize(_dFrameBody, _dGearBody, _dGearBody->GetPosition());
    _dFrameGearJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&def));
    gear.bodyA = _dWheelBody;
    gear.bodyB = _dGearBody;
    gear.joint1 = _dWheelJoint;
    gear.joint2 = _dFrameGearJoint;
    _dGearJoint = static_cast<b2GearJoint*>(world->CreateJoint(&gear));

    b2RevoluteJointDef hitch;
    hitch.Initialize(_frameBody, _dFrameBody, point("daughter_connectAnchor"));
    hitch.enableLimit = true;
    hitch.lowerAngle = -35.0f * kDeg;
    hitch.upperAngle = 35.0f * kDeg;
    _dConnectingJoint = world->CreateJoint(&hitch);

    // Son's basket, welded to the handlebar (a revolute joint locked at 0).
    b2RevoluteJointDef weld;
    weld.Initialize(_frameBody, _basketBody, point("son_crateAnchor"));
    weld.enableLimit = true;
    weld.lowerAngle = 0.0f;
    weld.upperAngle = 0.0f;
    _basketJoint = world->CreateJoint(&weld);
}

void MomBike::createDictionaries()
{
    _contactImpulseDict[_seatFixture] = _frameSmashLimit;
    _contactImpulseDict[_frontWheelFixture] = _wheelSmashLimit;
    _contactImpulseDict[_backWheelFixture] = _wheelSmashLimit;
    _contactImpulseDict[_dFrameFixture] = _daughterFrameSmashLimit;
    _contactImpulseDict[_basketFixtures[0]] = _basketSmashLimit;

    _contactAddSounds[_backWheelFixture] = "TireHit1";
    _contactAddSounds[_frontWheelFixture] = "TireHit2";
    _contactAddSounds[_seatFixture] = "BikeHit3";
    _contactAddSounds[_frameFixture] = "BikeHit2";
    _contactAddSounds[_forkFixture] = "BikeHit1";
    _contactAddSounds[_dWheelFixture] = "TireHit1";
    _contactAddSounds[_dFrameFixture] = "BikeHit3";
    _contactAddSounds[_dMidFixture] = "BikeHit2";
    _contactAddSounds[_dHandleFixture] = "BikeHit1";
    for (b2Fixture* f : _basketFixtures) {
        _contactAddSounds[f] = "BasketHit";
    }
}

// Character select: the back wheel locked (RoadBike), the daughter's wheel braked (Flash
// IrresponsibleMom.create in SessionCharacterMenu).
void MomBike::lockWheels()
{
    if (_backWheelJoint) {
        _backWheelJoint->EnableLimit(true);
        _backWheelJoint->SetLimits(0.0f, 0.0f);
    }
    if (_dWheelJoint) {
        _dWheelJoint->SetMotorSpeed(0.0f);
        _dWheelJoint->EnableMotor(true);
    }
}

// ---- riders ----------------------------------------------------------------------------------

// Flash BicycleGuy.createBodies / createJoints: legs are sensors while riding; pelvis on the
// saddle, hands on the handlebar, feet on the pedal gear.
void MomBike::addMom(CharacterB2D* mom)
{
    _mom = mom;
    if (!kidRiding(mom)) {  // (setVehicle has already added the kids)
        Vehicle::addCharacter(mom);
    }
    b2World* world = getWorld();

    limitJoint(mom->getHipJoint1(), mom->getPelvisBody(), mom->getUpperLeg1Body(), -110.0f, 10.0f);
    limitJoint(mom->getHipJoint2(), mom->getPelvisBody(), mom->getUpperLeg2Body(), -110.0f, 10.0f);
    limitJoint(mom->getElbowJoint1(), mom->getUpperArm1Body(), mom->getLowerArm1Body(), -60.0f, 0.0f);
    limitJoint(mom->getElbowJoint2(), mom->getUpperArm2Body(), mom->getLowerArm2Body(), -60.0f, 0.0f);
    limitJoint(mom->getNeckJoint(), mom->getChestBody(), mom->getHeadBody(), -20.0f, 0.0f);

    for (b2Body* leg : {mom->getUpperLeg1Body(), mom->getUpperLeg2Body(), mom->getLowerLeg1Body(),
                        mom->getLowerLeg2Body()}) {
        leg->GetFixtureList()->SetSensor(true);
    }

    b2RevoluteJointDef def;
    def.maxMotorTorque = _maxTorque;
    def.Initialize(_frameBody, mom->getPelvisBody(), mom->getPelvisBody()->GetPosition());
    addBodyVehicleJoint(mom->getPelvisBody(), world->CreateJoint(&def));
    def.Initialize(_frameBody, mom->getLowerArm1Body(), point("handleAnchor"));
    addBodyVehicleJoint(mom->getLowerArm1Body(), world->CreateJoint(&def));
    def.Initialize(_frameBody, mom->getLowerArm2Body(), point("handleAnchor"));
    addBodyVehicleJoint(mom->getLowerArm2Body(), world->CreateJoint(&def));
    def.Initialize(_gearBody, mom->getLowerLeg1Body(), point("gearAnchor1"));
    addBodyVehicleJoint(mom->getLowerLeg1Body(), world->CreateJoint(&def));
    def.Initialize(_gearBody, mom->getLowerLeg2Body(), point("gearAnchor2"));
    addBodyVehicleJoint(mom->getLowerLeg2Body(), world->CreateJoint(&def));
}

// Flash IMDaughter.createBodies / createJoints.
void MomBike::addDaughter(CharacterB2D* daughter)
{
    _daughter = daughter;
    if (!kidRiding(daughter)) {  // (setVehicle has already added the kids)
        Vehicle::addCharacter(daughter);
    }
    b2World* world = getWorld();

    _daughterExtraFilter.categoryBits = 0x104;
    _daughterExtraFilter.maskBits = 0xffff;
    _daughterExtraFilter.groupIndex = (int16)daughter->getGroupIndex();

    CharacterB2D* d = daughter;
    limitJoint(d->getHipJoint1(), d->getPelvisBody(), d->getUpperLeg1Body(), -110.0f, 10.0f);
    limitJoint(d->getHipJoint2(), d->getPelvisBody(), d->getUpperLeg2Body(), -110.0f, 10.0f);
    limitJoint(d->getElbowJoint1(), d->getUpperArm1Body(), d->getLowerArm1Body(), -60.0f, 0.0f);
    limitJoint(d->getElbowJoint2(), d->getUpperArm2Body(), d->getLowerArm2Body(), -60.0f, 0.0f);
    limitJoint(d->getNeckJoint(), d->getChestBody(), d->getHeadBody(), -20.0f, 0.0f);

    for (b2Body* leg : {d->getUpperLeg1Body(), d->getUpperLeg2Body(), d->getLowerLeg1Body(),
                        d->getLowerLeg2Body()}) {
        leg->GetFixtureList()->SetSensor(true);
    }

    b2RevoluteJointDef def;
    def.maxMotorTorque = _maxTorque;
    def.Initialize(_dFrameBody, d->getPelvisBody(), d->getPelvisBody()->GetPosition());
    addBodyVehicleJoint(d->getPelvisBody(), world->CreateJoint(&def));
    def.Initialize(_dFrameBody, d->getLowerArm1Body(), point("daughter_handleAnchor"));
    addBodyVehicleJoint(d->getLowerArm1Body(), world->CreateJoint(&def));
    def.Initialize(_dFrameBody, d->getLowerArm2Body(), point("daughter_handleAnchor"));
    addBodyVehicleJoint(d->getLowerArm2Body(), world->CreateJoint(&def));
    def.Initialize(_dGearBody, d->getLowerLeg1Body(), point("daughter_gearAnchor1"));
    addBodyVehicleJoint(d->getLowerLeg1Body(), world->CreateJoint(&def));
    def.Initialize(_dGearBody, d->getLowerLeg2Body(), point("daughter_gearAnchor2"));
    addBodyVehicleJoint(d->getLowerLeg2Body(), world->CreateJoint(&def));
}

// Flash IMSon.createJoints: the hands hold the basket rim, the pelvis slides on a short
// prismatic joint (0..0.25 Flash m along 25 degrees off vertical).
void MomBike::addSon(CharacterB2D* son)
{
    _son = son;
    if (!kidRiding(son)) {  // (setVehicle has already added the kids)
        Vehicle::addCharacter(son);
    }
    b2World* world = getWorld();

    _sonExtraFilter.categoryBits = 0x104;
    _sonExtraFilter.maskBits = 0xffff;
    _sonExtraFilter.groupIndex = (int16)son->getGroupIndex();

    CharacterB2D* s = son;
    limitJoint(s->getKneeJoint1(), s->getUpperLeg1Body(), s->getLowerLeg1Body(), 50.0f, 150.0f);
    limitJoint(s->getKneeJoint2(), s->getUpperLeg2Body(), s->getLowerLeg2Body(), 50.0f, 150.0f);
    limitJoint(s->getElbowJoint1(), s->getUpperArm1Body(), s->getLowerArm1Body(), -160.0f, -20.0f);
    limitJoint(s->getElbowJoint2(), s->getUpperArm2Body(), s->getLowerArm2Body(), -160.0f, -20.0f);
    limitJoint(s->getNeckJoint(), s->getChestBody(), s->getHeadBody(), -10.0f, 10.0f);

    // The hand end of a lower arm: the end of its box away from the elbow.
    auto handPoint = [](b2Body* arm, b2Joint* elbow) {
        b2PolygonShape* box = static_cast<b2PolygonShape*>(arm->GetFixtureList()->GetShape());
        float h = 0.0f;
        for (int i = 0; i < box->m_count; i++) {
            h = std::max(h, std::fabs(box->m_vertices[i].y));
        }
        b2Vec2 elbowLocal = arm->GetLocalPoint(elbow ? elbow->GetAnchorB() : arm->GetPosition());
        return arm->GetWorldPoint(b2Vec2(0.0f, elbowLocal.y > 0.0f ? -h : h));
    };
    b2RevoluteJointDef def;
    def.Initialize(_basketBody, s->getLowerArm1Body(), handPoint(s->getLowerArm1Body(), s->getElbowJoint1()));
    addBodyVehicleJoint(s->getLowerArm1Body(), world->CreateJoint(&def));
    def.Initialize(_basketBody, s->getLowerArm2Body(), handPoint(s->getLowerArm2Body(), s->getElbowJoint2()));
    addBodyVehicleJoint(s->getLowerArm2Body(), world->CreateJoint(&def));

    b2PrismaticJointDef slide;
    float a = 25.0f * kDeg;
    slide.Initialize(_basketBody, s->getPelvisBody(), s->getPelvisBody()->GetWorldCenter(),
                     b2Vec2(sinf(a), cosf(a)));
    slide.enableLimit = true;
    slide.lowerTranslation = 0.0f;
    slide.upperTranslation = 0.25f;
    addBodyVehicleJoint(s->getPelvisBody(), world->CreateJoint(&slide));
}

bool MomBike::kidRiding(CharacterB2D* kid)
{
    return kid && std::find(_characters.begin(), _characters.end(), kid) != _characters.end();
}

// Injuries: the joints of the hurt limb let go (Vehicle), then the rider-specific rules.
void MomBike::handleInjury(CharacterInjury injury, CharacterB2D* character)
{
    if (!kidRiding(character)) {
        return;
    }
    bool hip1 = injury == CharacterInjuryHip1Break;
    bool hip2 = injury == CharacterInjuryHip2Break;
    // Kids are thrown out by any head, chest or pelvis smash and a torso break (IMDaughter /
    // IMSon headSmash1, chestSmash, pelvisSmash, torsoBreak -> eject).
    if (character != _mom && (injury == CharacterInjuryHeadSmash || injury == CharacterInjuryChestSmash ||
                              injury == CharacterInjuryPelvisSmash || injury == CharacterInjuryTorsoBreak)) {
        handleFatalWound(character);
        return;
    }
    Vehicle::handleInjury(injury, character);
    // A leg that is no longer held by the pedals is no longer a sensor (IMDaughter.hipBreak).
    if ((hip1 || hip2) && character != _son && kidRiding(character)) {
        for (b2Body* b : {hip1 ? character->getUpperLeg1Body() : character->getUpperLeg2Body(),
                          hip1 ? character->getLowerLeg1Body() : character->getLowerLeg2Body()}) {
            if (b && b->GetFixtureList()) {
                if (character == _daughter) {
                    refilterKid(character, b->GetFixtureList(), true);
                } else {
                    b->GetFixtureList()->SetSensor(false);
                }
            }
        }
    }
}

// Flash checkEject: the mom and the daughter fall off once both hands and both feet have let go
// (the daughter's pelvis joint goes with her second foot); the son once both hands have.
void MomBike::checkStateOfCharacter(CharacterB2D* character)
{
    auto held = [this](b2Body* body) {
        auto it = _bodyVehicleJointDict.find(body);
        return it != _bodyVehicleJointDict.end() && it->second != nullptr;
    };
    CharacterB2D* c = character;
    if (c == _son) {
        if (!held(c->getLowerArm1Body()) && !held(c->getLowerArm2Body())) {
            ejectCharacter(c);
        }
        return;
    }
    bool feet = held(c->getLowerLeg1Body()) || held(c->getLowerLeg2Body());
    if (c == _daughter && !feet) {
        destroyJointsForBody(c->getPelvisBody());
    }
    if (!held(c->getLowerArm1Body()) && !held(c->getLowerArm2Body()) && !feet) {
        ejectCharacter(c);
    }
}

bool MomBike::ejectCharacter(CharacterB2D* character)
{
    if (!Vehicle::ejectCharacter(character)) {
        return false;
    }
    if (character == _mom) {
        momEject();
    } else if (character == _daughter) {
        daughterEject();
    } else if (character == _son) {
        sonEject();
    }
    return true;
}

// Flash BicycleGuy.eject: the bike's shapes now hit everything, the legs are solid again.
void MomBike::momEject()
{
    _momEjected = true;
    _ejected = true;
    forwardBackButtonsNull();
    leanButtonsNull();
    for (b2Body* body : {_frontWheelBody, _backWheelBody, _frameBody}) {
        if (!body) {
            continue;
        }
        for (b2Fixture* f = body->GetFixtureList(); f; f = f->GetNext()) {
            f->SetFilterData(_zeroFilter);
        }
    }
    for (b2Body* leg : {_mom->getUpperLeg1Body(), _mom->getUpperLeg2Body(), _mom->getLowerLeg1Body(),
                        _mom->getLowerLeg2Body()}) {
        if (leg && leg->GetFixtureList()) {
            leg->GetFixtureList()->SetSensor(false);
            leg->GetFixtureList()->Refilter();
        }
    }
}

// Flash IMDaughter.eject.
void MomBike::daughterEject()
{
    _daughterEjected = true;
    for (b2Fixture* f : {_dWheelFixture, _dSeatFixture, _dFrameFixture, _dMidFixture, _dHandleFixture}) {
        if (f && !_daughterSmashed) {
            f->SetFilterData(_zeroFilter);
        }
    }
    refilterKidParts(_daughter, true, false);
    if (_dWheelJoint) {
        _dWheelJoint->EnableMotor(false);
    }
}

// Flash IMSon.eject: thrown up out of the basket.
void MomBike::sonEject()
{
    _sonEjected = true;
    if (!_basketDetached) {
        refilterKidParts(_son, false, false);
    }
    b2Body* basket = _basketBody ? _basketBody : _son->getPelvisBody();
    float angle = basket->GetAngle() + (float)M_PI_2;
    b2Vec2 impulse(cosf(angle) * _sonEjectImpulse, sinf(angle) * _sonEjectImpulse);
    for (b2Body* body : {_son->getChestBody(), _son->getPelvisBody()}) {
        if (body) {
            body->ApplyLinearImpulse(impulse, body->GetWorldCenter(), true);
        }
    }
}

// ---- controls ----------------------------------------------------------------------------------
// The mom's bicycle as RoadBike (the mobile port's BicycleGuy): forward is a negative motor speed.

void MomBike::forwardButtonPressed()
{
    if (_mom->getDead() || _momEjected || !_backWheelJoint) {
        return;
    }
    if (_wheelContacts == 0) {
        _frontWheelJoint->EnableMotor(false);
        _backWheelJoint->EnableMotor(false);
    } else if (!_backWheelJoint->IsMotorEnabled()) {
        _backWheelJoint->EnableMotor(true);
        _frontWheelJoint->EnableMotor(true);
    }
    float speed = owb2::jointSpeed(_backWheelJoint);
    float motorSpeed = speed > 0.0f ? 0.0f : speed - (speed <= -_maxSpeed ? 0.0f : _accelStep);
    _backWheelJoint->SetMotorSpeed(motorSpeed);
    _frontWheelJoint->SetMotorSpeed(motorSpeed);
}

void MomBike::backButtonPressed()
{
    if (_mom->getDead() || _momEjected || !_backWheelJoint) {
        return;
    }
    if (_wheelContacts == 0) {
        _frontWheelJoint->EnableMotor(false);
        _backWheelJoint->EnableMotor(false);
    } else if (!_backWheelJoint->IsMotorEnabled()) {
        _backWheelJoint->EnableMotor(true);
        _frontWheelJoint->EnableMotor(true);
    }
    float speed = owb2::jointSpeed(_backWheelJoint);
    float motorSpeed = speed < 0.0f ? 0.0f : speed - (speed >= _maxSpeed ? 0.0f : -_accelStep);
    _backWheelJoint->SetMotorSpeed(motorSpeed);
    _frontWheelJoint->SetMotorSpeed(motorSpeed);
}

void MomBike::forwardBackButtonsNull()
{
    for (b2RevoluteJoint* joint : {_backWheelJoint, _frontWheelJoint}) {
        if (joint && joint->IsMotorEnabled()) {
            joint->EnableMotor(false);
        }
    }
    if (_currentPose == VehiclePoseForward || _currentPose == VehiclePoseBack) {
        setCurrentPose(VehiclePoseNone);
    }
}

// Brake (Flash spacePressedActions).
void MomBike::special1ButtonPressed()
{
    if (_momEjected || !_backWheelJoint) {
        return;
    }
    if (!_backWheelJoint->IsMotorEnabled()) {
        _backWheelJoint->EnableMotor(true);
        _frontWheelJoint->EnableMotor(true);
    }
    _backWheelJoint->SetMotorSpeed(0.0f);
    _frontWheelJoint->SetMotorSpeed(0.0f);
}

void MomBike::leanBackButtonPressed()
{
    if (_momEjected || !_frameBody) {
        return;
    }
    setCurrentPose(VehiclePoseLeanBack);
    float angularVelocity = _frameBody->GetAngularVelocity();
    double angle = _frameBody->GetAngle() + M_PI;
    float factor = b2Min(b2Max((double)((angularVelocity - _maxSpinAV) / -_maxSpinAV), 0.0), 1.0);
    double magnitude = _impulseMagnitude * s_timeStepOverFlashTimeStep;
    b2Vec2 impulse((float)(sin(angle) * magnitude * factor), -(float)(cos(angle) * magnitude * factor));
    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x + _impulseOffset, localCenter.y)), true);
}

void MomBike::leanForwardButtonPressed()
{
    if (_momEjected || !_frameBody) {
        return;
    }
    setCurrentPose(VehiclePoseLeanForward);
    float angularVelocity = _frameBody->GetAngularVelocity();
    double angle = _frameBody->GetAngle() + M_PI;
    float factor = b2Min(b2Max((angularVelocity + _maxSpinAV) / _maxSpinAV, 0.0f), 1.0f);
    double magnitude = _impulseMagnitude * s_timeStepOverFlashTimeStep;
    b2Vec2 impulse((float)(sin(angle) * magnitude * factor), -(float)(cos(angle) * magnitude * factor));
    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x - _impulseOffset, localCenter.y)), true);
}

void MomBike::leanBackPose()
{
    if (_momEjected) {
        return;
    }
    CharacterB2D* mom = _mom;
    mom->setJoint(mom->getNeckJoint(), 0.0f, 2.0f, 20.0f);
    if (mom->getElbowJoint1() && mom->getShoulderJoint1() && !mom->getUpperArm3Body()) {
        mom->setJoint(mom->getElbowJoint1(), -1.04f, 15.0f, 20.0f);
    }
    if (mom->getElbowJoint2() && mom->getShoulderJoint2() && !mom->getUpperArm4Body()) {
        mom->setJoint(mom->getElbowJoint2(), -1.04f, 15.0f, 20.0f);
    }
}

void MomBike::leanForwardPose()
{
    if (_momEjected) {
        return;
    }
    CharacterB2D* mom = _mom;
    mom->setJoint(mom->getNeckJoint(), -1.0f, 1.0f, 20.0f);
    if (mom->getElbowJoint1() && mom->getShoulderJoint1() && !mom->getUpperArm3Body()) {
        mom->setJoint(mom->getElbowJoint1(), 0.0f, 15.0f, 20.0f);
    }
    if (mom->getElbowJoint2() && mom->getShoulderJoint2() && !mom->getUpperArm4Body()) {
        mom->setJoint(mom->getElbowJoint2(), 0.0f, 15.0f, 20.0f);
    }
}

void MomBike::kidControls(unsigned char state)
{
    if (kidRiding(_daughter) && _dWheelJoint) {
        if (state & 0x03) {
            if (!_dWheelJoint->IsMotorEnabled()) {
                _dWheelJoint->EnableMotor(true);
            }
            float speed = owb2::jointSpeed(_dWheelJoint);
            float motorSpeed;
            if (state & 0x01) {  // up: pedal forward (Flash positive = mobile negative)
                motorSpeed = speed > 0.0f ? 0.0f : (speed > -_daughterMaxSpeed ? speed - _daughterAccelStep : speed);
            } else {
                motorSpeed = speed < 0.0f ? 0.0f : (speed < _daughterMaxSpeed ? speed + _daughterAccelStep : speed);
            }
            _dWheelJoint->SetMotorSpeed(motorSpeed);
        } else if (_dWheelJoint->IsMotorEnabled()) {
            _dWheelJoint->EnableMotor(false);
        }
        if (state & 0x10) {  // space: brake
            _dWheelJoint->EnableMotor(true);
            _dWheelJoint->SetMotorSpeed(0.0f);
        }
    }
    if ((state & 0x20) && kidRiding(_son)) {
        ejectCharacter(_son);
    }
    if ((state & 0x40) && kidRiding(_daughter)) {
        ejectCharacter(_daughter);
    }
}

// ---- per frame -----------------------------------------------------------------------------------

void MomBike::actions()
{
    Vehicle::actions();
}

void MomBike::paint()
{
    float ptm = getPtm();
    for (const Painted& p : _painted) {
        b2Vec2 c = p.atCentre ? p.body->GetWorldCenter() : p.body->GetPosition();
        p.sprite->setPosition(Vec2(c.x * ptm, c.y * ptm));
        p.sprite->setRotation(-CC_RADIANS_TO_DEGREES(p.body->GetAngle()) - p.angleOffset);
    }
}

void MomBike::checkJoints()
{
    if (!_daughterDetached && checkRevJoint(_dConnectingJoint, _daughterDetachLimit)) {
        detachDaughter();
    }
    if (!_basketDetached && checkRevJoint(_basketJoint, _basketDetachLimit)) {
        detachBasket();
    }
}

void MomBike::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    Vehicle::beginContact(fixture, otherFixture, contact);
}

void MomBike::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    Vehicle::endContact(fixture, otherFixture, contact);
}

// Every frame shape reports to its bike's smash shape (Flash contactFrameResultHandler /
// contactBasketResultHandler); wheels keep the hit normal for the tyre fold.
void MomBike::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                        const b2ContactImpulse* impulse)
{
    float normalImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        normalImpulse = b2Max(normalImpulse, impulse->normalImpulses[1]);
    }
    b2Fixture* key = nullptr;
    if (fixture == _seatFixture || fixture == _frameFixture || fixture == _forkFixture) {
        key = _seatFixture;
    } else if (fixture == _dSeatFixture || fixture == _dFrameFixture || fixture == _dHandleFixture ||
               fixture == _dMidFixture || fixture == _dEndFixture) {
        key = _dFrameFixture;
    } else if (std::find(_basketFixtures.begin(), _basketFixtures.end(), fixture) != _basketFixtures.end()) {
        key = _basketFixtures[0];
    } else if (fixture == _frontWheelFixture || fixture == _backWheelFixture) {
        key = fixture;
    }
    if (!key) {
        return;
    }
    auto limit = _contactImpulseDict.find(key);
    if (limit == _contactImpulseDict.end() || normalImpulse <= limit->second) {
        return;
    }
    VehicleContact& result = _results[key];
    if (normalImpulse > result.impulse) {
        result.impulse = normalImpulse;
        b2WorldManifold world;
        contact->GetWorldManifold(&world);
        // the normal points from fixture A to B; make it point into this fixture's body
        result.normal = contact->GetFixtureA() == fixture ? world.normal : -world.normal;
    }
}

void MomBike::handleContactResults()
{
    std::map<b2Fixture*, VehicleContact> results;
    results.swap(_results);
    auto hit = [&results](b2Fixture* f, VehicleContact* out) {
        auto it = results.find(f);
        if (f && it != results.end() && it->second.impulse > 0.0f) {
            if (out) {
                *out = it->second;
            }
            return true;
        }
        return false;
    };
    VehicleContact c;
    if (!_basketSmashed && !_basketFixtures.empty() && hit(_basketFixtures[0], &c)) {
        basketSmash();
    }
    if (!_frameSmashed && hit(_seatFixture, &c)) {
        frameSmash();
    }
    if (_frontWheelFixture && hit(_frontWheelFixture, &c)) {
        wheelSmash(true, c.normal);
    }
    if (_backWheelFixture && hit(_backWheelFixture, &c)) {
        wheelSmash(false, c.normal);
    }
    if (!_daughterSmashed && hit(_dFrameFixture, &c)) {
        daughterFrameSmash();
    }
}

void MomBike::debugFunction(int value)
{
    if (!_frameSmashed) {
        frameSmash();
    }
}

// ---- breaking ------------------------------------------------------------------------------------

// A loose piece of a smashed frame: a new body with the frame's position and velocity and one
// polygon from the guide (Flash: density 2, zero filter), painted at its mass centre.
b2Body* MomBike::makePiece(b2Body* from, const std::string& prefix, int count, const std::string& frame,
                           const std::string& hitSound)
{
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = from->GetPosition();
    def.angle = from->GetAngle();
    b2Body* body = getWorld()->CreateBody(&def);
    b2FixtureDef fixture;
    fixture.density = 2.0f;
    fixture.friction = 0.3f;
    fixture.restitution = 0.1f;
    fixture.filter = _zeroFilter;
    b2Vec2 vertices[b2_maxPolygonVertices];
    for (int i = 0; i < count; i++) {
        // the frame's own local frame = the guide's (bodies start on the guide origin, angle 0)
        b2Vec2 p = point(prefix + patch::to_string(i + 1));
        vertices[i] = b2Vec2(p.x - _origin.x, p.y - _origin.y);
    }
    b2PolygonShape polygon;
    polygon.Set(vertices, count);
    fixture.shape = &polygon;
    b2Fixture* f = body->CreateFixture(&fixture);
    body->ResetMassData();
    body->SetLinearVelocity(from->GetLinearVelocity());
    body->SetAngularVelocity(from->GetAngularVelocity());
    Sprite* s = sprite(frame, getSession()->getVehicleBackground(), 3);
    paintBody(body, s, true);
    if (!hitSound.empty()) {
        addToBeginContact(f);
        _contactAddSounds[f] = hitSound;
    }
    return body;
}

// Flash IrresponsibleMom.frameSmash: the kids' rides come off, the mom is thrown off and the frame
// breaks into fork, broken frame and seat (BicycleGuy.frameSmash).
void MomBike::frameSmash()
{
    _frameSmashed = true;
    _vehicleSmashed = true;
    if (!_daughterDetached) {
        detachDaughter();
    }
    if (!_basketDetached) {
        detachBasket();
    }
    if (kidRiding(_mom)) {
        ejectCharacter(_mom);
    }
    for (b2Fixture* f : {_seatFixture, _frameFixture, _forkFixture}) {
        removePostSolve(f);
        removeBeginContact(f);
        fixtureWillBeDestroyed(f);
        _contactImpulseDict.erase(f);
    }
    stopSoundsForBody(_frameBody);

    b2World* world = getWorld();
    world->DestroyJoint(_gearJoint);
    world->DestroyJoint(_frontWheelJoint);
    world->DestroyJoint(_backWheelJoint);
    world->DestroyJoint(_frameGearJoint);
    _gearJoint = nullptr;
    _frontWheelJoint = nullptr;
    _backWheelJoint = nullptr;
    _frameGearJoint = nullptr;
    _wheelJoints.clear();
    _wheelJointSpeedDict.clear();

    makePiece(_frameBody, "forkVert", 3, "fork", "");
    b2Body* broken = makePiece(_frameBody, "brokenVert", 4, "brokenFrame", "BikeHit2");
    makePiece(_frameBody, "seatVert", 3, "seat", "");
    unpaint(_frameBody);
    _frameSprite->setVisible(false);
    world->DestroyBody(_frameBody);
    _frameBody = nullptr;
    _seatFixture = _frameFixture = _forkFixture = nullptr;
    _gearBody->GetFixtureList()->SetFilterData(_zeroFilter);
    if (gameplay()) {
        createBodySound("BikeSmash1", broken, 1.0f, false);
    }
}

// Flash BicycleGuy.front/backWheelSmash: the tyre folds into a box across the hit normal.
void MomBike::wheelSmash(bool front, b2Vec2 normal)
{
    b2Fixture*& fixture = front ? _frontWheelFixture : _backWheelFixture;
    b2Body* body = front ? _frontWheelBody : _backWheelBody;
    removePostSolve(fixture);
    _contactImpulseDict.erase(fixture);

    float radius = fixture->GetShape()->m_radius;
    float angle = atan2f(normal.y, normal.x) - body->GetAngle();
    b2PolygonShape box;
    box.SetAsBox(radius * 0.46f, radius * 0.92f, b2Vec2(0.0f, 0.0f), angle);
    b2FixtureDef def;
    def.shape = &box;
    def.density = 2.0f;
    def.friction = 0.3f;
    def.restitution = 0.1f;
    if (_momEjected) {
        def.filter = _zeroFilter;
    } else {
        def.filter.categoryBits = 0x201;
    }
    removeBeginContact(fixture);
    removeEndContact(fixture);
    fixtureWillBeDestroyed(fixture);
    body->DestroyFixture(fixture);
    fixture = body->CreateFixture(&def);
    body->ResetMassData();
    addContactWheel(fixture);
    _wheelContacts = 0;

    for (Painted& p : _painted) {
        if (p.body == body) {
            Node* parent = p.sprite->getParent();
            int z = p.sprite->getLocalZOrder();
            p.sprite->removeFromParent();
            p.sprite = sprite(front ? "frontWheelBroken" : "backWheelBroken", parent, z);
            p.angleOffset = -CC_RADIANS_TO_DEGREES(angle);
            (front ? _frontWheelSprite : _backWheelSprite) = p.sprite;
        }
    }
    if (gameplay()) {
        createBodySound("BikeTireSmash1", body, 1.0f, false);
    }
}

// Flash IMDaughter.detachFrame: the hitch lets go; the daughter now hits her mom.
void MomBike::detachDaughter()
{
    if (_daughterDetached) {
        return;
    }
    _daughterDetached = true;
    destroyJoint(_dConnectingJoint);
    if (_daughterSmashed) {
        return;
    }
    if (_daughter) {
        refilterKidParts(_daughter, true, true);
    }
    if (_dEndFixture) {
        _dEndFixture->SetSensor(false);
        _dEndFixture->Refilter();
    }
    if (gameplay()) {
        createBodySound("StrapSnap1", _dFrameBody, 1.0f, false);
    }
}

// Flash IMDaughter.frameSmash.
void MomBike::daughterFrameSmash()
{
    if (kidRiding(_daughter)) {
        ejectCharacter(_daughter);
    }
    _daughterSmashed = true;
    _daughterDetached = true;
    destroyJoint(_dConnectingJoint);
    for (b2Fixture* f : {_dSeatFixture, _dFrameFixture, _dHandleFixture, _dMidFixture, _dEndFixture}) {
        removePostSolve(f);
        removeBeginContact(f);
        fixtureWillBeDestroyed(f);
        _contactImpulseDict.erase(f);
    }
    b2World* world = getWorld();
    world->DestroyJoint(_dGearJoint);
    world->DestroyJoint(_dWheelJoint);
    world->DestroyJoint(_dFrameGearJoint);
    _dGearJoint = nullptr;
    _dWheelJoint = nullptr;
    _dFrameGearJoint = nullptr;

    makePiece(_dFrameBody, "daughter_broken2Vert", 3, "daughterFork", "");
    b2Body* broken = makePiece(_dFrameBody, "daughter_brokenVert", 4, "daughterBrokenFrame", "BikeHit2");
    makePiece(_dFrameBody, "daughter_seatVert", 3, "daughterSeat", "");
    unpaint(_dFrameBody);
    _dFrameSprite->setVisible(false);
    stopSoundsForBody(_dFrameBody);
    world->DestroyBody(_dFrameBody);
    _dFrameBody = nullptr;
    _dSeatFixture = _dFrameFixture = _dHandleFixture = _dMidFixture = _dEndFixture = nullptr;
    if (gameplay()) {
        createBodySound("BikeSmash1", broken, 1.0f, false);
    }
}

// Flash IMSon.detachBasket: the basket comes off the handlebar; its shapes are replaced by the
// shorter crate2 ones (zero filter), the son now hits his mom.
void MomBike::detachBasket()
{
    if (_basketDetached) {
        return;
    }
    _basketDetached = true;
    destroyJoint(_basketJoint);
    if (_basketSmashed) {
        return;
    }
    for (b2Fixture* f : _basketFixtures) {
        removePostSolve(f);
        removeBeginContact(f);
        fixtureWillBeDestroyed(f);
        _contactImpulseDict.erase(f);
        _basketBody->DestroyFixture(f);
    }
    _basketFixtures.clear();
    if (kidRiding(_son)) {
        refilterKidParts(_son, false, false);
    }
    b2FixtureDef def;
    def.density = 1.0f;
    def.friction = 0.3f;
    def.restitution = 0.1f;
    def.filter = _zeroFilter;
    for (const char* prefix : {"son_crate2Right", "son_crate2Bottom", "son_crate2Left"}) {
        b2Vec2 vertices[4];
        for (int i = 0; i < 4; i++) {
            b2Vec2 p = point(std::string(prefix) + patch::to_string(i + 1));
            vertices[i] = b2Vec2(p.x - _origin.x, p.y - _origin.y);
        }
        b2PolygonShape polygon;
        polygon.Set(vertices, 4);
        def.shape = &polygon;
        b2Fixture* f = _basketBody->CreateFixture(&def);
        _basketFixtures.push_back(f);
        addToPostSolve(f);
        addToBeginContact(f);
        _contactAddSounds[f] = "BasketHit";
    }
    _contactImpulseDict[_basketFixtures[0]] = _basketSmashLimit;
    _basketBody->ResetMassData();
}

// Flash IMSon.basketSmash: the son is thrown out and the basket bursts into pieces.
void MomBike::basketSmash()
{
    _basketSmashed = true;
    if (kidRiding(_son)) {
        ejectCharacter(_son);
    }
    _basketDetached = true;
    destroyJoint(_basketJoint);
    for (b2Fixture* f : _basketFixtures) {
        removePostSolve(f);
        removeBeginContact(f);
        fixtureWillBeDestroyed(f);
        _contactImpulseDict.erase(f);
    }
    _basketFixtures.clear();
    b2Vec2 centre = _basketBody->GetWorldCenter();
    unpaint(_basketBody);
    _basketSprite->setVisible(false);
    stopSoundsForBody(_basketBody);
    // (destroying the basket body takes the son's hand and pelvis joints with it: he was ejected
    // first, which already destroyed them)
    getWorld()->DestroyBody(_basketBody);
    _basketBody = nullptr;

    // createPointBurst("basketPieces", x, y, 5, 20, 30): 30 pieces, 5 px apart, speed range 20.
    std::vector<std::string> frames;
    for (int i = 1; i <= 15; i++) {
        frames.push_back(_name + "_basketPiece_" + patch::to_string(i) + ".png");
    }
    if (restored::FlashParticles* particles = restored::FlashParticles::forSession(getSession())) {
        particles->burst(frames, centre, b2Vec2(0.0f, 0.0f), 5.0f, 20.0f, 30);
    }
    if (gameplay()) {
        createPositionSound("BasketSmash", Vec2(centre.x, centre.y), 1.0f, false);
    }
}
