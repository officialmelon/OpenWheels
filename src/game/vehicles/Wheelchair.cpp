#include "Wheelchair.h"

#include <cmath>
#include <functional>
#include <string>

#include "CharacterB2D.h"
#include "LevelB2D.h"
#include "Patch.h"
#include "QueryCallback.h"
#include "Session.h"
#include "Settings.h"
#include "Sound.h"
#include "SoundController.h"
#include "platform/compat/Box2DFloat.h"

USING_NS_CC;

// @00641c18
Wheelchair::Wheelchair()
{
    _frontWheelFixture = nullptr;
    _backWheelFixture = nullptr;
}

// @00641d34 (D1, tail call to Vehicle's), @00641d38 (D0)
Wheelchair::~Wheelchair()
{
}

// @00641d5c
bool Wheelchair::init(Vec2 position, std::string name, int groupID)
{
    _maxSpeed = 20.0f;
    _accelStep = 0.1f;
    _maxTorque = 20.0f;
    _wheelSoundName = "BikeLoop1";
    bool ok = Vehicle::init(position, name, groupID);
    if (ok)
    {
        loadSpriteFrames(LevelItemTextureIdMineExplosion);
    }
    return ok;
}

// @00641e5c
void Wheelchair::createSprites()
{
    Node* foreground = getSession()->getCharacterForeground();

    _frameSprite = Sprite::createWithSpriteFrameName(_name + "_frame.png");
    _frameSprite->setAnchorPoint(Vec2(0.35f, 0.835f));
    foreground->addChild(_frameSprite);

    _backWheelSprite = Sprite::createWithSpriteFrameName(_name + "_wheel.png");
    foreground->addChild(_backWheelSprite);

    _frontWheelSprite = Sprite::createWithSpriteFrameName(_name + "_frontWheel.png");
    foreground->addChild(_frontWheelSprite);

    _jetSprite = Sprite::createWithSpriteFrameName(_name + "_jet.png");
    foreground->addChild(_jetSprite);

    _flameSprite = Sprite::createWithSpriteFrameName(_name + "_flame.png");
    _flameSprite->setPosition(Vec2(_flameSprite->getTextureRect().size.width * -0.5f,
                                   _jetSprite->getTextureRect().size.height * 0.5f));
    _flameSprite->setVisible(false);
    _jetSprite->addChild(_flameSprite);
}

// @006423b0
void Wheelchair::createBodies()
{
    b2World* world = Settings::getInstance()->getCurrentSession()->getWorld();

    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    ValueMap chair1Shape = bodies.at("chair1Shape").asValueMap();
    ValueMap chair2Shape = bodies.at("chair2Shape").asValueMap();
    ValueMap chair3Shape = bodies.at("chair3Shape").asValueMap();
    ValueMap bigWheelShape = bodies.at("bigWheelShape").asValueMap();
    ValueMap smallWheelShape = bodies.at("smallWheelShape").asValueMap();

    // wheels
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.angularDamping = 1.0f;
    bodyDef.allowSleep = false;

    Vec2 position = PointFromString(smallWheelShape.at("pos").asString());
    bodyDef.position = b2Vec2(position.x + _origin.x, position.y + _origin.y);
    _frontWheelBody = world->CreateBody(&bodyDef);

    b2CircleShape circle;
    b2FixtureDef fixtureDef;
    fixtureDef.userData = this;
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.3f;
    fixtureDef.density = 5.0f;
    fixtureDef.isSensor = false;
    fixtureDef.filter.categoryBits = 0x0401;
    fixtureDef.filter.maskBits = 0xffff;
    fixtureDef.filter.groupIndex = 0;

    circle.m_radius = smallWheelShape.at("radius").asFloat();
    fixtureDef.shape = &circle;
    _frontWheelFixture = _frontWheelBody->CreateFixture(&fixtureDef);
    _frontWheelBody->ResetMassData();

    position = PointFromString(bigWheelShape.at("pos").asString());
    bodyDef.position = b2Vec2(position.x + _origin.x, position.y + _origin.y);
    _backWheelBody = world->CreateBody(&bodyDef);

    circle.m_radius = bigWheelShape.at("radius").asFloat();
    fixtureDef.shape = &circle;
    _backWheelFixture = _backWheelBody->CreateFixture(&fixtureDef);
    _backWheelBody->ResetMassData();

    // chair
    fixtureDef.userData = this;
    fixtureDef.density = 2.0f;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;

    b2BodyDef frameBodyDef;
    frameBodyDef.type = b2_dynamicBody;
    frameBodyDef.position = b2Vec2(_origin.x, _origin.y);
    _frameBody = world->CreateBody(&frameBodyDef);

    b2PolygonShape box;

    Size size = SizeFromString(chair1Shape.at("size").asString());
    position = PointFromString(chair1Shape.at("pos").asString());
    box.SetAsBox(size.width, size.height, b2Vec2(position.x, position.y), 0.0f);
    fixtureDef.shape = &box;
    _frame1Fixture = _frameBody->CreateFixture(&fixtureDef);

    size = SizeFromString(chair2Shape.at("size").asString());
    position = PointFromString(chair2Shape.at("pos").asString());
    box.SetAsBox(size.width, size.height, b2Vec2(position.x, position.y), 0.0f);
    fixtureDef.shape = &box;
    _frame2Fixture = _frameBody->CreateFixture(&fixtureDef);

    size = SizeFromString(chair3Shape.at("size").asString());
    position = PointFromString(chair3Shape.at("pos").asString());
    box.SetAsBox(size.width, size.height, b2Vec2(position.x, position.y), 0.0f);
    fixtureDef.shape = &box;
    _frame3Fixture = _frameBody->CreateFixture(&fixtureDef);

    _frameBody->ResetMassData();

    addToPostSolve(_frame1Fixture);
    addToPostSolve(_frame2Fixture);
    addToPostSolve(_frame3Fixture);

    _frontWheelBody->SetUserData(_frontWheelSprite);
    _backWheelBody->SetUserData(_backWheelSprite);
    _frameBody->SetUserData(_frameSprite);

    // The back wheel and the jet are positioned by paint().
    getLevel()->addToPaintBody(_frameBody);
    getLevel()->addToPaintBody(_frontWheelBody);
    getLevel()->addToPaintItem(this);
}

// @006431ac
void Wheelchair::lockWheels()
{
    _backWheelJoint->EnableLimit(true);
    _backWheelJoint->SetLimits(0.0f, 0.0f);
}

// @006431e0
void Wheelchair::createJoints()
{
    b2World* world = getWorld();

    b2RevoluteJointDef jointDef;
    jointDef.maxMotorTorque = _maxTorque;

    jointDef.Initialize(_frameBody, _frontWheelBody, _frontWheelBody->GetWorldCenter());
    _frontWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));

    jointDef.Initialize(_frameBody, _backWheelBody, _backWheelBody->GetWorldCenter());
    _backWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
}

// @006432b4
void Wheelchair::createDictionaries()
{
    _contactImpulseDict[_frame1Fixture] = _frameSmashLimit;
    _contactImpulseDict[_frontWheelFixture] = _wheelSmashLimit;
    _contactImpulseDict[_backWheelFixture] = _wheelSmashLimit;

    _contactAddSounds[_frame1Fixture] = "ChairHit1";
    _contactAddSounds[_frame2Fixture] = "ChairHit2";
    _contactAddSounds[_frame3Fixture] = "ChairHit3";
    _contactAddSounds[_backWheelFixture] = "TireHit1";
}

// @00643764
void Wheelchair::addCharacter(CharacterB2D* character)
{
    Vehicle::addCharacter(character);

    b2World* world = Settings::getInstance()->getCurrentSession()->getWorld();

    // Hips locked into a seated pose: 90..110 degrees relative to the current leg angle.
    float legAngle = character->getUpperLeg1Body()->GetAngle() - character->getPelvisBody()->GetAngle();
    character->getHipJoint1()->SetLimits(1.57079637f - legAngle, 1.91986215f - legAngle);
    legAngle = character->getUpperLeg2Body()->GetAngle() - character->getPelvisBody()->GetAngle();
    character->getHipJoint2()->SetLimits(1.57079637f - legAngle, 1.91986215f - legAngle);

    b2RevoluteJointDef jointDef;

    jointDef.Initialize(_frameBody, character->getChestBody(),
                        character->getChestBody()->GetWorldCenter());
    _chairChestJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    _bodyVehicleJointDict[character->getChestBody()] = _chairChestJoint;

    jointDef.Initialize(_frameBody, character->getPelvisBody(),
                        character->getPelvisBody()->GetWorldCenter());
    _chairPelvisJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    _bodyVehicleJointDict[character->getPelvisBody()] = _chairPelvisJoint;

    jointDef.Initialize(_frameBody, character->getUpperLeg1Body(),
                        character->getUpperLeg1Body()->GetWorldCenter());
    _vehicleUpperLeg1 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    _bodyVehicleJointDict[character->getUpperLeg1Body()] = _vehicleUpperLeg1;

    jointDef.Initialize(_frameBody, character->getUpperLeg2Body(),
                        character->getUpperLeg2Body()->GetWorldCenter());
    _vehicleUpperLeg2 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    _bodyVehicleJointDict[character->getUpperLeg2Body()] = _vehicleUpperLeg2;

    int zOrder = static_cast<Node*>(character->getLowerLeg1Body()->GetUserData())->getLocalZOrder();
    _frameSprite->setLocalZOrder(zOrder);
    _frontWheelSprite->setLocalZOrder(zOrder);
    _backWheelSprite->setLocalZOrder(zOrder);
    _jetSprite->setLocalZOrder(zOrder);

    b2Filter armFilter;
    armFilter.categoryBits = 0x0204;
    armFilter.maskBits = 0x0208;
    armFilter.groupIndex = _groupID;
    character->getLowerArm1Body()->GetFixtureList()->SetFilterData(armFilter);
    character->getLowerArm2Body()->GetFixtureList()->SetFilterData(armFilter);
}

// @00643ccc
void Wheelchair::forwardButtonPressed()
{
    if (!_backWheelJoint->IsMotorEnabled())
    {
        _backWheelJoint->EnableMotor(true);
    }
    float speed = owb2::jointSpeed(_backWheelJoint);
    float newSpeed = 0.0f;
    if (speed <= 0.0f)
    {
        newSpeed = speed - ((speed <= -_maxSpeed) ? 0.0f : _accelStep);
    }
    _backWheelJoint->SetMotorSpeed(newSpeed);
    setCurrentPose(VehiclePoseForward);
}

// @00643d48
void Wheelchair::backButtonPressed()
{
    if (!_backWheelJoint->IsMotorEnabled())
    {
        _backWheelJoint->EnableMotor(true);
    }
    float speed = owb2::jointSpeed(_backWheelJoint);
    float newSpeed = 0.0f;
    if (speed >= 0.0f)
    {
        newSpeed = speed + ((_maxSpeed <= speed) ? -0.0f : _accelStep);
    }
    _backWheelJoint->SetMotorSpeed(newSpeed);
    setCurrentPose(VehiclePoseBack);
}

// @00643dc0
void Wheelchair::forwardBackButtonsNull()
{
    if (_backWheelJoint->IsMotorEnabled())
    {
        _backWheelJoint->EnableMotor(false);
    }
    if (_currentPose == VehiclePoseForward || _currentPose == VehiclePoseBack)
    {
        setCurrentPose(VehiclePoseNone);
    }
}

// @00643e20
void Wheelchair::leanForwardButtonPressed()
{
    setCurrentPose(VehiclePoseLeanForward);

    float angularVelocity = _frameBody->GetAngularVelocity();
    float angle = _frameBody->GetAngle();
    float spinFactor = (angularVelocity + _maxSpinAV) / _maxSpinAV;
    if (spinFactor <= 0.0f)
    {
        spinFactor = 0.0f;
    }
    if (spinFactor > 1.0f)
    {
        spinFactor = 1.0f;
    }
    float impulse = _impulseMagnitudeMax * s_timeStepOverFlashTimeStep;
    b2Vec2 localCenter = _frameBody->GetLocalCenter();

    _frameBody->ApplyLinearImpulse(
        b2Vec2(sinf(angle) * impulse * spinFactor, -cosf(angle) * impulse * spinFactor),
        _frameBody->GetWorldPoint(b2Vec2(localCenter.x + 1.0f, localCenter.y)), true);
    _frameBody->ApplyLinearImpulse(
        b2Vec2(sin(angle + M_PI) * impulse * spinFactor, -cos(angle + M_PI) * impulse * spinFactor),
        _frameBody->GetWorldPoint(b2Vec2(localCenter.x - 1.0f, localCenter.y)), true);
}

// @00644034
void Wheelchair::leanBackButtonPressed()
{
    setCurrentPose(VehiclePoseLeanBack);

    float angularVelocity = _frameBody->GetAngularVelocity();
    float angle = _frameBody->GetAngle();
    float spinFactor = fmin(fmax((angularVelocity - _maxSpinAV) / -_maxSpinAV, 0.0), 1.0);
    float impulse = _impulseMagnitudeMax * s_timeStepOverFlashTimeStep;
    b2Vec2 localCenter = _frameBody->GetLocalCenter();

    _frameBody->ApplyLinearImpulse(
        b2Vec2(sinf(angle) * impulse * spinFactor, -cosf(angle) * impulse * spinFactor),
        _frameBody->GetWorldPoint(b2Vec2(localCenter.x - 1.0f, localCenter.y)), true);
    _frameBody->ApplyLinearImpulse(
        b2Vec2(sin(angle + M_PI) * impulse * spinFactor, -cos(angle + M_PI) * impulse * spinFactor),
        _frameBody->GetWorldPoint(b2Vec2(localCenter.x + 1.0f, localCenter.y)), true);
}

// @00644250
void Wheelchair::leanButtonsNull()
{
    if (_currentPose == VehiclePoseLeanForward || _currentPose == VehiclePoseLeanBack)
    {
        setCurrentPose(VehiclePoseNone);
    }
}

// @00644274
void Wheelchair::special1ButtonPressed()
{
    if (_chairSmashed)
    {
        return;
    }
    if (!_firing)
    {
        _firing = true;
        _flameSprite->setVisible(true);
        if (_jetSound == nullptr)
        {
            _jetSound = createBodySound("jetBlast2", _backWheelBody, 1.0f, true);
            if (_jetSound)
            {
                // @00646d0c (ZN10Wheelchair21special1ButtonPressedEvE3$_0)
                _jetSound->setFinishCallback([this](int&) { jetSoundStopped(); });
                _jetSound->setMaxVolume(0.0f);
            }
        }
        if (_jetSound)
        {
            _jetSound->fadeTo(1.0f, 0.2f, false);
        }
    }
    setCurrentPose(VehiclePoseSpecial);
}

// @00644438
void Wheelchair::special1ButtonNull()
{
    if (_firing)
    {
        _firing = false;
        _flameSprite->setVisible(false);
        if (_jetSound)
        {
            _jetSound->fadeTo(0.0f, 0.2f, true);
        }
    }
    if (_currentPose == VehiclePoseSpecial)
    {
        setCurrentPose(VehiclePoseNone);
    }
}

// @006444bc
void Wheelchair::jetSoundStopped()
{
    _jetSound = nullptr;
}

// @006444c4
void Wheelchair::paint()
{
    Node* wheelSprite = static_cast<Node*>(_backWheelBody->GetUserData());
    b2Vec2 position = _backWheelBody->GetPosition();
    wheelSprite->setPosition(Vec2(position.x * getPtm(), position.y * getPtm()));
    wheelSprite->setRotation(_backWheelBody->GetAngle() * -57.29578f - _backWheelAngOffset);

    if (!_chairSmashed)
    {
        b2Vec2 center = _backWheelBody->GetWorldCenter();
        _jetSprite->setPosition(Vec2(center.x * getPtm(), center.y * getPtm()));
        _jetSprite->setRotation((_frameBody->GetAngle() + _jetAngle) * -57.29578f);
    }
}

// @00644604
void Wheelchair::actions()
{
    Vehicle::actions();

    if (_firing)
    {
        float jetAngle = _jetAngle;
        Sprite* flame = _flameSprite;
        float chairAngle = _frameBody->GetAngle();
        flame->setScale(CCRANDOM_0_1() * 0.4f + 0.8f, CCRANDOM_0_1() * 0.2f + 0.9f);

        float angle = chairAngle + jetAngle;
        b2Vec2 impulse(cosf(angle) * 2.0f, sinf(angle) * 2.0f);
        _backWheelBody->ApplyLinearImpulse(s_timeStepOverFlashTimeStep * impulse,
                                           _frameBody->GetWorldCenter(), true);
    }
}

// @00644754
bool Wheelchair::ejectCharacter(CharacterB2D* character)
{
    bool ejected = Vehicle::ejectCharacter(character);
    if (ejected)
    {
        forwardBackButtonsNull();
        leanButtonsNull();
        setCurrentPose(VehiclePoseNone);

        _frontWheelBody->GetFixtureList()->SetFilterData(_zeroFilter);
        _backWheelBody->GetFixtureList()->SetFilterData(_zeroFilter);
        for (b2Fixture* fixture = _frameBody->GetFixtureList(); fixture; fixture = fixture->GetNext())
        {
            fixture->SetFilterData(_zeroFilter);
        }

        b2Fixture* armFixture = character->getLowerArm1Body()->GetFixtureList();
        if (armFixture->GetFilterData().maskBits == 0x0208)
        {
            armFixture->SetFilterData(_defaultFilter);
        }
        armFixture = character->getLowerArm2Body()->GetFixtureList();
        if (armFixture->GetFilterData().maskBits == 0x0208)
        {
            armFixture->SetFilterData(_defaultFilter);
        }
    }
    return ejected;
}

// @0064484c
void Wheelchair::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                           const b2ContactImpulse* impulse)
{
    if (fixture == _frame1Fixture || fixture == _frame2Fixture || fixture == _frame3Fixture)
    {
        float maxImpulse = impulse->normalImpulses[0];
        if (contact->GetManifold()->pointCount == 2)
        {
            maxImpulse = (maxImpulse > impulse->normalImpulses[1]) ? maxImpulse
                                                                    : impulse->normalImpulses[1];
        }
        // All three chair fixtures report into the entries of the first one.
        if (_contactImpulseDict[_frame1Fixture] < maxImpulse &&
            _contactResultBufferDict[_frame1Fixture].impulse < maxImpulse)
        {
            _contactResultBufferDict[_frame1Fixture].impulse = maxImpulse;
            _contactResultBufferDict[_frame1Fixture].position = contact->GetManifold()->localNormal;
        }
    }
    else if (fixture == _jetFixture)
    {
        float maxImpulse = impulse->normalImpulses[0];
        if (contact->GetManifold()->pointCount == 2)
        {
            maxImpulse = (maxImpulse > impulse->normalImpulses[1]) ? maxImpulse
                                                                    : impulse->normalImpulses[1];
        }
        if (_contactImpulseDict[fixture] < maxImpulse &&
            _contactResultBufferDict[_jetFixture].impulse < maxImpulse)
        {
            _contactResultBufferDict[_jetFixture].impulse = maxImpulse;
            _contactResultBufferDict[_jetFixture].position = contact->GetManifold()->localNormal;
        }
    }
    else if (fixture == _fueltankFixture)
    {
        float maxImpulse = impulse->normalImpulses[0];
        if (contact->GetManifold()->pointCount == 2)
        {
            maxImpulse = (maxImpulse > impulse->normalImpulses[1]) ? maxImpulse
                                                                    : impulse->normalImpulses[1];
        }
        if (_contactImpulseDict[fixture] < maxImpulse &&
            _contactResultBufferDict[_fueltankFixture].impulse < maxImpulse)
        {
            _contactResultBufferDict[_fueltankFixture].impulse = maxImpulse;
            _contactResultBufferDict[_fueltankFixture].position = contact->GetManifold()->localNormal;
        }
    }
}

// @00645028
void Wheelchair::handleContactResults()
{
    if (_contactResultBufferDict[_frame1Fixture].impulse > 0.0f)
    {
        frameSmash(_contactResultBufferDict[_frame1Fixture].impulse,
                   _contactResultBufferDict[_frame1Fixture].position);
    }
    else if (_contactResultBufferDict[_fueltankFixture].impulse > 0.0f)
    {
        // The arguments are read from the chair's entry, not the fuel tank's (both lookups use
        // _frame1Fixture in the binary, @00645234/@006454b4); fueltankSmash ignores them anyway.
        fueltankSmash(_contactResultBufferDict[_frame1Fixture].impulse,
                      _contactResultBufferDict[_frame1Fixture].position);
    }
    else if (_contactResultBufferDict[_jetFixture].impulse > 0.0f)
    {
        jetSmash(_contactResultBufferDict[_jetFixture].impulse,
                 _contactResultBufferDict[_jetFixture].position);
    }
    _contactResultBufferDict.clear();
}

// @00645644
void Wheelchair::frameSmash(float impulse, b2Vec2 point)
{
    ejectAllCharacters();

    Node* foreground = getSession()->getCharacterForeground();

    _contactImpulseDict.erase(_frame1Fixture);
    _contactImpulseDict.erase(_frame2Fixture);
    _contactImpulseDict.erase(_frame3Fixture);
    _contactResultBufferDict.erase(_frame1Fixture);
    removePostSolve(_frame1Fixture);
    removePostSolve(_frame2Fixture);
    removePostSolve(_frame3Fixture);
    _chairSmashed = true;

    b2World* world = getWorld();
    world->DestroyJoint(_frontWheelJoint);
    world->DestroyJoint(_backWheelJoint);

    b2Fixture* fixture;
    while ((fixture = _frameBody->GetFixtureList()) != nullptr)
    {
        fixtureWillBeDestroyed(fixture);
        _frameBody->DestroyFixture(fixture);
    }

    // The broken chair: one box on the old body.
    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    b2Vec2 frameCenter = localCenter + b2Vec2(0.064f, -0.576f);
    b2PolygonShape box;
    box.SetAsBox(0.32f, 0.36f, frameCenter, 0.0f);
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &box;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 2.0f;
    fixtureDef.filter = _zeroFilter;
    _frameBody->CreateFixture(&fixtureDef);
    _frameBody->ResetMassData();

    _frameSprite->removeFromParentAndCleanup(false);
    _frameSprite = Sprite::createWithSpriteFrameName("wheelchair_brokenFrame.png");
    Rect rect = _frameSprite->getTextureRect();
    rect.size.width = rect.size.width / getPtm();
    rect.size.height = rect.size.height / getPtm();
    _frameSprite->setAnchorPoint(
        Vec2(0.5f - frameCenter.x / rect.size.width, 0.5f - frameCenter.y / rect.size.height));
    foreground->addChild(_frameSprite);

    // The handle falls off.
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    _frameBody->SetUserData(_frameSprite);
    bodyDef.position = _frameBody->GetWorldPoint(localCenter + b2Vec2(-0.24f, 0.704f));
    b2Body* handleBody = world->CreateBody(&bodyDef);
    float frameAngle = _frameBody->GetAngle();
    box.SetAsBox(0.04f, 0.28f);
    handleBody->CreateFixture(&fixtureDef);
    handleBody->ResetMassData();
    Sprite* handleSprite = Sprite::createWithSpriteFrameName("wheelchair_handle.png");
    handleSprite->setAnchorPoint(Vec2(0.783f, 0.528f));
    foreground->addChild(handleSprite);
    handleBody->SetUserData(handleSprite);

    // As in the binary (@00645c8c): the chair's velocity is copied onto the chair itself (the handle
    // keeps a zero velocity); the same values are given to the jet below.
    b2Vec2 linearVelocity = _frameBody->GetLinearVelocity();
    float angularVelocity = _frameBody->GetAngularVelocity();
    _frameBody->SetLinearVelocity(linearVelocity);
    _frameBody->SetAngularVelocity(angularVelocity);
    getLevel()->addToPaintBody(handleBody);

    // The fuel tank becomes its own body (fueltankSmash when hit hard enough).
    bodyDef.position = _frameBody->GetWorldPoint(localCenter + b2Vec2(0.064f, 0.424f));
    bodyDef.angle = frameAngle;
    _fueltankBody = world->CreateBody(&bodyDef);
    box.SetAsBox(0.24f, 0.072f);
    _fueltankFixture = _fueltankBody->CreateFixture(&fixtureDef);
    _fueltankBody->ResetMassData();
    _fueltankFixture->SetUserData(this);
    addToPostSolve(_fueltankFixture);
    Sprite* tankSprite = Sprite::createWithSpriteFrameName("wheelchair_tank.png");
    foreground->addChild(tankSprite);
    _fueltankBody->SetUserData(tankSprite);
    getLevel()->addToPaintBody(_fueltankBody);
    _contactImpulseDict[_fueltankFixture] = _fueltankSmashLimit;

    // So does the jet (jetSmash).
    b2BodyDef jetBodyDef;
    jetBodyDef.type = b2_dynamicBody;
    jetBodyDef.angle = _jetAngle;
    jetBodyDef.position = _backWheelBody->GetWorldCenter();
    _jetBody = world->CreateBody(&jetBodyDef);
    box.SetAsBox(0.168f, 0.128f);
    _jetFixture = _jetBody->CreateFixture(&fixtureDef);
    _jetFixture->SetUserData(this);
    addToPostSolve(_jetFixture);
    _jetBody->SetUserData(_jetSprite);
    _jetBody->SetAngularVelocity(angularVelocity);
    _jetBody->SetLinearVelocity(linearVelocity);
    _contactImpulseDict[_jetFixture] = _jetSmashLimit;
    getLevel()->addToPaintBody(_jetBody);

    if (_firing)
    {
        _firing = false;
        _flameSprite->setVisible(false);
        if (_jetSound)
        {
            _jetSound->fadeTo(0.0f, 0.2f, false);
            _jetSound = nullptr;
        }
    }

    // As in the binary (@006460dc): the sound is placed at the chair's local centre, not at a world
    // position.
    Settings::getInstance()->getSoundController()->createPositionSound(
        "MetalSmashMedium", Vec2(localCenter.x, localCenter.y), 1.0f, false);

    _frame1Fixture = nullptr;
    _frame2Fixture = nullptr;
    _frame3Fixture = nullptr;
}

// @006461c0
void Wheelchair::fueltankSmash(float impulse, b2Vec2 point)
{
    b2World* world = getWorld();
    b2Vec2 center = _fueltankBody->GetWorldCenter();
    float angle = _fueltankBody->GetAngle();

    _contactImpulseDict.erase(_fueltankFixture);
    _contactResultBufferDict.erase(_fueltankFixture);
    removePostSolve(_fueltankFixture);

    Vector<SpriteFrame*> frames(40);
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    for (int i = 1; i <= 40; i++)
    {
        frames.pushBack(cache->getSpriteFrameByName("mineExplosion_" + patch::to_string(i) + ".png"));
    }
    Animate* animate = Animate::create(Animation::createWithSpriteFrames(frames, 1.0f / 60.0f, 1));

    Node* foreground = getSession()->getCharacterForeground();
    Vec2 position(center.x * getPtm(), center.y * getPtm());
    _explosionSprite = Sprite::createWithSpriteFrameName("mineExplosion_1.png");
    _explosionSprite->setScale(2.0f);
    _explosionSprite->setAnchorPoint(Vec2(0.5f, 0.04f));
    _explosionSprite->setPosition(position);
    _explosionSprite->setRotation(angle * -57.29578f);
    foreground->addChild(_explosionSprite);
    _explosionSprite->runAction(Sequence::create(
        animate, CallFunc::create(std::bind(&Wheelchair::fueltankExplosionComplete, this)), nullptr));

    blastBodies(center, 1.0f);
    createPositionSound("MineExplosion", position, 1.0f, false);

    static_cast<Node*>(_fueltankBody->GetUserData())->removeFromParentAndCleanup(false);
    getLevel()->removeFromPaintBody(_fueltankBody);
    world->DestroyBody(_fueltankBody);
    _fueltankBody = nullptr;
    _fueltankFixture = nullptr;
}

// @00646824
void Wheelchair::jetSmash(float impulse, b2Vec2 point)
{
    b2World* world = getWorld();

    _contactImpulseDict.erase(_jetFixture);
    _contactResultBufferDict.erase(_jetFixture);
    removePostSolve(_jetFixture);

    createPositionSound("MetalSmashLight",
                        Vec2(_jetBody->GetWorldCenter().x, _jetBody->GetWorldCenter().y), 1.0f, false);

    static_cast<Node*>(_jetBody->GetUserData())->removeFromParentAndCleanup(false);
    getLevel()->removeFromPaintBody(_jetBody);
    world->DestroyBody(_jetBody);
    _jetBody = nullptr;
    _jetFixture = nullptr;
    _jetSprite = nullptr;
}

// @00646a8c
void Wheelchair::fueltankExplosionComplete()
{
    _explosionSprite->removeFromParent();
    _explosionSprite = nullptr;
}

// @00646abc
void Wheelchair::blastBodies(b2Vec2 position, float radius)
{
    // `radius`: the query box half-size and the impulse fall-off distance.
    QueryCallback callback;
    b2AABB aabb;
    aabb.lowerBound = b2Vec2(position.x - radius, position.y - radius);
    aabb.upperBound = b2Vec2(position.x + radius, position.y + radius);
    getWorld()->QueryAABB(&callback, aabb);

    for (unsigned int i = 0; i < callback._fixtures.size(); i++)
    {
        b2Body* body = callback._fixtures[i]->GetBody();
        if (body->GetType() == b2_staticBody)
        {
            continue;
        }
        b2Vec2 bodyCenter = body->GetWorldCenter();
        b2Vec2 delta = bodyCenter - position;
        float blastAngle = atan2f(delta.y, delta.x);
        b2Vec2 direction(cosf(blastAngle), sinf(blastAngle));
        float falloff = 1.0f - fminf(radius, delta.Length()) / radius;
        b2Vec2 impulse = falloff * direction;
        impulse = 10.0f * impulse;
        body->ApplyLinearImpulse(impulse, bodyCenter, true);
    }
}
