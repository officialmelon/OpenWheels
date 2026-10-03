// PersonalTransporter: Business Guy's self-balancing personal transporter (iOS `Segway`).
// Reconstructed from libMyGame.so 1.1.3 (arm64); see PersonalTransporter.h for the layout notes and
// docs/modules/M2.md for the analysis notes.
//
// create() is inline in the header (kept copy @00589224 in BusinessGuy's TU). There is no
// user-declared constructor or destructor; the compiler-generated deleting destructor @006023b0 is
// emitted with this TU's vtable.

#include "PersonalTransporter.h"

#include "cocos2d.h"

#include "CharacterB2D.h"
#include "LevelB2D.h"
#include "Session.h"
#include "Settings.h"
#include "Sound.h"
#include "SoundController.h"

USING_NS_CC;

// @005ffb1c
bool PersonalTransporter::init(Vec2 position, std::string name, int groupID)
{
    if (Vehicle::init(position, name, groupID))
    {
        Settings::getInstance()->getSoundController()->preloadSound("SegwayLoop1");
        _maxTorque = 20.0f;
        _maxSpeed = -40.0f;
        _accelStep = 1.0f;
        getSession()->getLevel()->addToPaintItem(this);
        _motorSound = nullptr;
        _wheelSoundVolume = 0.2f;
        return true;
    }
    return false;
}

// @005ffc7c
void PersonalTransporter::addCharacter(CharacterB2D* character)
{
    Vehicle::addCharacter(character);
    b2World* world = Settings::getInstance()->getCurrentSession()->getWorld();

    // The rider's joint limits are made relative to the pose he was created in.
    float angle = character->getUpperLeg1Body()->GetAngle() - character->getPelvisBody()->GetAngle();
    character->getHipJoint1()->SetLimits(-0.174532920f - angle, 0.872664630f - angle);
    angle = character->getUpperLeg2Body()->GetAngle() - character->getPelvisBody()->GetAngle();
    character->getHipJoint2()->SetLimits(-0.174532920f - angle, 0.872664630f - angle);
    angle = character->getLowerArm1Body()->GetAngle() - character->getUpperArm1Body()->GetAngle();
    character->getElbowJoint1()->SetLimits(0.0f - angle, 1.04719758f - angle);
    angle = character->getLowerArm2Body()->GetAngle() - character->getUpperArm2Body()->GetAngle();
    character->getElbowJoint2()->SetLimits(0.0f - angle, 1.04719758f - angle);
    angle = character->getHeadBody()->GetAngle() - character->getChestBody()->GetAngle();
    character->getNeckJoint()->SetLimits(-0.349065840f - angle, 0.0f - angle);

    ValueMap joints = _bodiesDict.at("joints").asValueMap();

    // Hands on the handle bar.
    Vec2 handleAnchor = PointFromString(joints.at("handleAnchor").asString()) + _origin;
    b2Vec2 anchor(handleAnchor.x, handleAnchor.y);
    b2RevoluteJointDef jointDef;
    jointDef.enableLimit = true;
    jointDef.lowerAngle = -1.74532926f;
    jointDef.upperAngle = 0.174532920f;
    jointDef.maxMotorTorque = 100000.0f;
    jointDef.Initialize(_frameBody, character->getLowerArm1Body(), anchor);
    b2Joint* joint = world->CreateJoint(&jointDef);
    addBodyVehicleJoint(character->getLowerArm1Body(), joint);
    jointDef.Initialize(_frameBody, character->getLowerArm2Body(), anchor);
    joint = world->CreateJoint(&jointDef);
    addBodyVehicleJoint(character->getLowerArm2Body(), joint);

    // Feet on the platform.
    Vec2 footAnchor = PointFromString(joints.at("footAnchor").asString()) + _origin;
    anchor = b2Vec2(footAnchor.x, footAnchor.y);
    jointDef.lowerAngle = -0.174532920f;
    jointDef.upperAngle = 0.174532920f;
    jointDef.Initialize(_frameBody, character->getLowerLeg1Body(), anchor);
    jointDef.collideConnected = false;
    joint = world->CreateJoint(&jointDef);
    addBodyVehicleJoint(character->getLowerLeg1Body(), joint);
    jointDef.Initialize(_frameBody, character->getLowerLeg2Body(), anchor);
    jointDef.collideConnected = false;
    joint = world->CreateJoint(&jointDef);
    addBodyVehicleJoint(character->getLowerLeg2Body(), joint);
}

// @00600278
void PersonalTransporter::createSprites()
{
    Node* background = getSession()->getVehicleBackground();
    _frameSprite = Sprite::createWithSpriteFrameName(_name + "_frame.png");
    _frameSprite->setAnchorPoint(Vec2(-0.324999988f, 1.39499998f));
    background->addChild(_frameSprite);

    Node* foreground = getSession()->getVehicleForeground();
    _wheelCoverSprite = Sprite::createWithSpriteFrameName(_name + "_wheelCover.png");
    _wheelCoverSprite->setAnchorPoint(Vec2(0.5f, 0.319999993f));
    foreground->addChild(_wheelCoverSprite);

    _shockSprite = Sprite::createWithSpriteFrameName(_name + "_shock.png");
    _shockSprite->setAnchorPoint(Vec2(0.5f, 1.0f));
    foreground->addChild(_shockSprite);

    _wheelSprite = Sprite::createWithSpriteFrameName(_name + "_wheel.png");
    foreground->addChild(_wheelSprite);

    _innerSprite = Sprite::createWithSpriteFrameName(_name + "_inner.png");
    Size wheelSize = _wheelSprite->getTextureRect().size;
    _innerSprite->setPosition(Vec2(wheelSize.width * 0.5f, wheelSize.height * 0.5f));
    _innerSprite->setAnchorPoint(Vec2(0.5f, 2.15000010f));
    _wheelSprite->addChild(_innerSprite);
}

// @00600820
void PersonalTransporter::createFilters()
{
}

// @00600824
void PersonalTransporter::createBodies()
{
    Session* session = Settings::getInstance()->getCurrentSession();
    b2World* world = Settings::getInstance()->getCurrentSession()->getWorld();

    b2PolygonShape polygon;
    b2CircleShape circle;

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = b2Vec2(_origin.x, _origin.y);

    b2FixtureDef fixtureDef;
    fixtureDef.density = 5.0f;
    fixtureDef.friction = 0.300000012f;
    fixtureDef.restitution = 0.100000001f;
    fixtureDef.filter.categoryBits = 0x202;

    _frameBody = world->CreateBody(&bodyDef);

    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    ValueMap wheelShape = bodies.at("wheelShape").asValueMap();
    ValueMap frameShape = bodies.at("frameShape").asValueMap();
    ValueMap handleShape = bodies.at("handleShape").asValueMap();
    ValueMap standShape = bodies.at("standShape").asValueMap();

    // Frame and handle: two boxes on the frame body ("size" is used as the half extents).
    Vec2 pos = PointFromString(frameShape.at("pos").asString());
    Size size = SizeFromString(frameShape.at("size").asString());
    float rot = frameShape.at("rot").asFloat();
    polygon.SetAsBox(size.width, size.height, b2Vec2(pos.x, pos.y), rot);
    fixtureDef.shape = &polygon;
    _frameFixture = _frameBody->CreateFixture(&fixtureDef);

    pos = PointFromString(handleShape.at("pos").asString());
    size = SizeFromString(handleShape.at("size").asString());
    rot = handleShape.at("rot").asFloat();
    polygon.SetAsBox(size.width, size.height, b2Vec2(pos.x, pos.y), rot);
    fixtureDef.shape = &polygon;
    _handleFixture = _frameBody->CreateFixture(&fixtureDef);
    addToPostSolve(_handleFixture);

    // Stand and shock: two sensor bodies at the stand position; the shock collides with nothing.
    // "rot" is read but not used (axis-aligned box centred on the body).
    pos = PointFromString(standShape.at("pos").asString());
    size = SizeFromString(standShape.at("size").asString());
    rot = standShape.at("rot").asFloat();
    polygon.SetAsBox(size.width, size.height);
    fixtureDef.shape = &polygon;
    fixtureDef.isSensor = true;
    Vec2 standPosition = pos + _origin;
    bodyDef.position = b2Vec2(standPosition.x, standPosition.y);
    _standBody = world->CreateBody(&bodyDef);
    _standBody->SetFixedRotation(true);
    _standBody->CreateFixture(&fixtureDef);
    _shockBody = world->CreateBody(&bodyDef);
    _shockBody->SetFixedRotation(true);
    fixtureDef.filter.maskBits = 0;
    _shockBody->CreateFixture(&fixtureDef);
    fixtureDef.isSensor = false;
    fixtureDef.filter.maskBits = 0xffff;

    // Wheel.
    float radius = wheelShape.at("radius").asFloat();
    circle = b2CircleShape();
    circle.m_radius = radius;
    fixtureDef.shape = &circle;
    fixtureDef.density = 5.0f;
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.300000012f;
    fixtureDef.filter.groupIndex = static_cast<int16>(_groupID);
    fixtureDef.filter.categoryBits = 0x104;
    fixtureDef.filter.maskBits = 0x10c;
    _wheelBody = createBody(&wheelShape, _origin);
    _frontWheelFixture = _wheelBody->CreateFixture(&fixtureDef);

    _wheelBody->ResetMassData();
    _shockBody->ResetMassData();
    _standBody->ResetMassData();
    _frameBody->ResetMassData();

    _frameBody->SetUserData(_frameSprite);
    _standBody->SetUserData(_wheelCoverSprite);
    session->getLevel()->addToPaintBody(_frameBody);
    session->getLevel()->addToPaintBody(_standBody);
    addToBeginContact(_frontWheelFixture);
}

// @00601468
void PersonalTransporter::lockWheels()
{
    _wheelJoint->EnableLimit(true);
    _wheelJoint->SetLimits(0.0f, 0.0f);
}

// @0060149c
void PersonalTransporter::createFixtures()
{
}

// @006014a0
void PersonalTransporter::createJoints()
{
    b2World* world = Settings::getInstance()->getCurrentSession()->getWorld();

    // Shock slides vertically under the stand (special1 = jump).
    b2PrismaticJointDef shockJointDef;
    b2Vec2 anchor = _wheelBody->GetPosition();
    shockJointDef.enableLimit = true;
    shockJointDef.maxMotorForce = 1000.0f;
    shockJointDef.Initialize(_standBody, _shockBody, anchor, b2Vec2(0.0f, 1.0f));
    _shockJoint = static_cast<b2PrismaticJoint*>(world->CreateJoint(&shockJointDef));

    // Wheel axle on the shock.
    b2RevoluteJointDef jointDef;
    jointDef.maxMotorTorque = 20.0f;
    jointDef.Initialize(_shockBody, _wheelBody, anchor);
    _wheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    addWheelJoint(_wheelJoint, _wheelBody);

    // Frame hinged to the stand (lean).
    jointDef.enableLimit = true;
    jointDef.lowerAngle = -0.261799395f;
    jointDef.upperAngle = 0.261799395f;
    ValueMap joints = _bodiesDict.at("joints").asValueMap();
    Vec2 frameAnchor = PointFromString(joints.at("frameAnchor").asString()) + _origin;
    anchor = b2Vec2(frameAnchor.x, frameAnchor.y);
    jointDef.maxMotorTorque = 4000.0f;
    jointDef.Initialize(_standBody, _frameBody, anchor);
    _standFrame = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
}

// @006017f4
void PersonalTransporter::setLimits()
{
}

// @006017f8
void PersonalTransporter::addContactListeners()
{
}

// @006017fc
void PersonalTransporter::createDictionaries()
{
    _contactImpulseDict[_handleFixture] = 20.0f;
    _contactAddSounds[_frontWheelFixture] = "TireHit1";
}

// @0060196c
void PersonalTransporter::frameSmash(float impulse, b2Vec2 point)
{
    _vehicleSmashed = true;
    ejectAllCharacters();
    removePostSolve(_handleFixture);
    getWorld()->DestroyJoint(_standFrame);
    _standFrame = nullptr;
    getLevel()->removeFromPaintBody(_standBody);
    getWorld()->DestroyBody(_standBody);
    getWorld()->DestroyBody(_shockBody);
    _shockBody = nullptr;
    _standBody = nullptr;
    for (b2Fixture* fixture = _frameBody->GetFixtureList(); fixture; fixture = fixture->GetNext())
    {
        fixture->SetFilterData(_zeroFilter);
    }
    _wheelBody->GetFixtureList()->SetFilterData(_zeroFilter);
    _shockSprite->setVisible(false);
}

// @00601a3c
void PersonalTransporter::checkStateOfCharacter(CharacterB2D* character)
{
    if (_bodyVehicleJointDict[character->getLowerArm1Body()] == nullptr &&
        _bodyVehicleJointDict[character->getLowerArm2Body()] == nullptr)
    {
        ejectCharacter(character);
    }
}

// @00601be4
void PersonalTransporter::handleContactResults()
{
    float impulse = _contactResultBufferDict[_handleFixture].impulse;
    if (impulse != 0.0f)
    {
        // The second argument is never initialised by the original (b2Vec2() leaves it as is).
        frameSmash(impulse, b2Vec2());
    }
    _contactResultBufferDict.clear();
}

// @00601ce8
void PersonalTransporter::ejectAllCharacters()
{
    Vehicle::ejectAllCharacters();
}

// @00601cec
bool PersonalTransporter::ejectCharacter(CharacterB2D* character)
{
    if (!_ejected)
    {
        _ejected = true;
        special1ButtonNull();
        forwardBackButtonsNull();
        leanButtonsNull();
        b2Filter filter;
        filter.categoryBits = 0x104;
        filter.maskBits = 0xffff;
        filter.groupIndex = -2;
        for (b2Fixture* fixture = _frameBody->GetFixtureList(); fixture; fixture = fixture->GetNext())
        {
            fixture->SetFilterData(filter);
        }
        _frontWheelFixture->SetFilterData(filter);
        return Vehicle::ejectCharacter(character);
    }
    return false;
}

// @00601dcc
void PersonalTransporter::addMotorSound()
{
    if (!_motorSound)
    {
        _motorSound = createBodySound("SegwayLoop1", _wheelBody, 1.0f, true);
        if (_motorSound)
        {
            _motorSound->setMaxVolume(0.0f);
            _motorSound->fadeTo(0.25f, 0.2f, false);
        }
    }
    else
    {
        _motorSound->fadeTo(0.25f, 0.2f, false);
    }
}

// @00601ef8
void PersonalTransporter::stopMotorSound()
{
    if (_motorSound)
    {
        _motorSound->fadeTo(0.0f, 0.2f, true);
        _motorSound = nullptr;
    }
}

// @00601f38
void PersonalTransporter::actions()
{
    Vehicle::actions();
}

// @00601f3c
void PersonalTransporter::paint()
{
    float ptm = Settings::getInstance()->getCurrentSession()->getPtmRatio();
    Vec2 wheelPosition(ptm * _wheelBody->GetPosition().x, ptm * _wheelBody->GetPosition().y);
    if (_vehicleSmashed)
    {
        // The stand body (which carried the wheel cover sprite) is gone: follow the wheel.
        _wheelSprite->setPosition(wheelPosition);
        _wheelCoverSprite->setPosition(wheelPosition);
        _innerSprite->setRotation(_wheelBody->GetAngle() * -57.2957802f);
    }
    else
    {
        _wheelSprite->setPosition(wheelPosition);
        _innerSprite->setRotation(_wheelBody->GetAngle() * -57.2957802f);
        b2Vec2 shockAnchor = _shockJoint->GetAnchorA();
        _shockSprite->setPosition(Vec2(ptm * shockAnchor.x, ptm * shockAnchor.y));
        float shockScale =
            -(_shockJoint->GetJointTranslation() * ptm) / _shockSprite->getTextureRect().size.height;
        _shockSprite->setScaleY(shockScale);
    }
}

// @0060209c
void PersonalTransporter::forwardButtonPressed()
{
    Vehicle::forwardButtonPressed();
    addMotorSound();
}

// @006020c0
void PersonalTransporter::backButtonPressed()
{
    Vehicle::backButtonPressed();
    addMotorSound();
}

// @006020e4
void PersonalTransporter::forwardBackButtonsNull()
{
    Vehicle::forwardBackButtonsNull();
    stopMotorSound();
}

// @00602128
void PersonalTransporter::leanForwardButtonPressed()
{
    setJoint(_standFrame, -0.52f, 20.0f, 10.0f);
}

// @00602144
void PersonalTransporter::leanBackButtonPressed()
{
    setJoint(_standFrame, 0.0f, 20.0f, 10.0f);
}

// @00602158
void PersonalTransporter::leanButtonsNull()
{
    setJoint(_standFrame, -0.26f, 20.0f, 10.0f);
}

// @00602174
void PersonalTransporter::special1ButtonPressed()
{
    if (!_shockJoint->IsMotorEnabled())
    {
        // Pull the shock up into the stand, then (below) push it back out: the jump.
        _shockJoint->SetMotorSpeed(-5.0f);
        _shockJoint->SetLimits(-_jumpTranslation, 0.0f);
        _shockJoint->EnableMotor(true);
        createBodySound("SegwayJump", _wheelBody, 1.0f, false);
    }
    else if (_shockJoint->GetMotorSpeed() < 0.0f)
    {
        if (_shockJoint->GetJointTranslation() < -_jumpTranslation)
        {
            _shockJoint->SetMotorSpeed(1.0f);
        }
    }
    else if (_shockJoint->GetMotorSpeed() > 0.0f && _shockJoint->GetJointTranslation() > 0.0f)
    {
        _shockJoint->EnableMotor(false);
        _shockJoint->SetLimits(0.0f, 0.0f);
        _shockJoint->SetMotorSpeed(0.0f);
    }
}

// @00602300
void PersonalTransporter::special1ButtonNull()
{
    if (_shockJoint->IsMotorEnabled())
    {
        if (_shockJoint->GetMotorSpeed() < 0.0f)
        {
            if (_shockJoint->GetJointTranslation() < -_jumpTranslation)
            {
                _shockJoint->SetMotorSpeed(1.0f);
            }
        }
        else if (_shockJoint->GetMotorSpeed() > 0.0f && _shockJoint->GetJointTranslation() > 0.0f)
        {
            _shockJoint->EnableMotor(false);
            _shockJoint->SetLimits(0.0f, 0.0f);
            _shockJoint->SetMotorSpeed(0.0f);
        }
    }
}

// @006023a4
void PersonalTransporter::leanBackPose()
{
}

// @006023a8
void PersonalTransporter::leanForwardPose()
{
}

// @006023ac
void PersonalTransporter::noLeanBackPose()
{
}
