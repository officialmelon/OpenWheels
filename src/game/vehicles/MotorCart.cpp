#include "MotorCart.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "BurstEmitter.h"
#include "CharacterB2D.h"
#include "EmitterNode.h"
#include "FlowEmitter.h"
#include "LevelB2D.h"
#include "Patch.h"
#include "Session.h"
#include "Sound.h"

USING_NS_CC;

// @005f4694
MotorCart::MotorCart()
    : _frontWheelBody(nullptr),
      _backWheelBody(nullptr),
      _backShockBody(nullptr),
      _frontShockBody(nullptr),
      _frameBody(nullptr),
      _mainFixture1(nullptr),
      _mainFixture2(nullptr),
      _mainFixture3(nullptr),
      _mainFixture4(nullptr),
      _cartFixture(nullptr),
      _handleFixture(nullptr),
      _shaftFixture(nullptr),
      _sodaFixture(nullptr),
      _crackerFixture(nullptr),
      _backWheelJoint(nullptr),
      _frontWheelJoint(nullptr),
      _backShockJoint(nullptr),
      _frontShockJoint(nullptr),
      _frameSprite(nullptr),
      _cartSprite(nullptr),
      _frontShockSprite(nullptr),
      _backShockSprite(nullptr),
      _frontWheelSprite(nullptr),
      _backWheelSprite(nullptr),
      _handleSprite(nullptr),
      _smashedSodaSprite(nullptr),
      _frontWheelSound(nullptr),
      _motorSound(nullptr),
      _frameAnchor(0.215f, 1.05f),
      _mcJumpTrans(0.32f),
      _impulseMagnitudeMax(1.25f),
      _impulseOffset(1.0f),
      _maxSpinAV(5.0f),
      _cartSmashLimit(100.0f),
      _frameSmashLimit(200.0f),
      _groceriesSmashLimit(1.0f)
{
    _frontWheelFixture = nullptr;
    _backWheelFixture = nullptr;
}

// @005f47a8 (D1), @005f47e4 (D0)
MotorCart::~MotorCart()
{
}

// @005f4808
bool MotorCart::init(Vec2 position, std::string name, int groupID)
{
    _maxSpeed = -50.0f;
    _accelStep = 1.0f;
    _maxTorque = 100000.0f;
    bool result = Vehicle::init(position, name, groupID);
    if (result) {
        getSession()->getLevel()->addToPaintItem(this);
        _wheelSoundVolume = 0.2f;
    }
    return result;
}

// @005f4908
void MotorCart::lockWheels()
{
    _backWheelJoint->EnableLimit(true);
    _backWheelJoint->SetLimits(0.0f, 0.0f);
}

// @005f493c
void MotorCart::createSprites()
{
    Node* vehicleBackground = getSession()->getVehicleBackground();

    _frontShockSprite = Sprite::createWithSpriteFrameName("motor_cart_shock.png");
    _backShockSprite = Sprite::createWithSpriteFrameName("motor_cart_shock.png");
    vehicleBackground->addChild(_frontShockSprite);
    vehicleBackground->addChild(_backShockSprite);

    _frontWheelSprite = Sprite::createWithSpriteFrameName("motor_cart_wheel.png");
    vehicleBackground->addChild(_frontWheelSprite);
    _backWheelSprite = Sprite::createWithSpriteFrameName("motor_cart_wheel.png");
    vehicleBackground->addChild(_backWheelSprite);

    _frameSprite = Sprite::createWithSpriteFrameName("motor_cart_frame.png");
    _frameSprite->setAnchorPoint(_frameAnchor);
    vehicleBackground->addChild(_frameSprite);
    addHandleToFrameSprite(_frameSprite);

    _cartSprite = Sprite::createWithSpriteFrameName("motor_cart_cart.png");
    getSession()->getCharacterForeground()->addChild(_cartSprite);
    _cartSprite->setAnchorPoint(Vec2(-0.25f, -0.25f));
    _cartSprite->setLocalZOrder(1000);
}

// @005f4c3c
void MotorCart::addHandleToFrameSprite(Sprite* frameSprite)
{
    _handleSprite = Sprite::createWithSpriteFrameName("motor_cart_handle.png");
    frameSprite->addChild(_handleSprite);
    _handleSprite->setPosition(Vec2(getPtm() * 1.1552f, getPtm() * 0.7464f));
}

// @005f4d64  (no callers in 1.1.3)
void MotorCart::addCartToFrameSprite(Sprite* frameSprite)
{
    _cartSprite = Sprite::createWithSpriteFrameName("motor_cart_cart.png");
    frameSprite->addChild(_cartSprite);
    _cartSprite->setPosition(Vec2(3.072f, 1.472f));
}

// @005f4e5c
void MotorCart::createDictionaries()
{
    _contactImpulseDict[_mainFixture1] = _frameSmashLimit;
    _contactImpulseDict[_cartFixture] = _cartSmashLimit;
    _contactImpulseDict[_crackerFixture] = _groceriesSmashLimit;
    _contactImpulseDict[_sodaFixture] = _groceriesSmashLimit;

    _contactAddSounds[_backWheelFixture] = "CarTire1";
    _contactAddSounds[_frontWheelFixture] = "CarTire1";
    _contactAddSounds[_mainFixture1] = "BikeHit3";
    _contactAddSounds[_cartFixture] = "BikeHit1";
}

// @005f53ac
void MotorCart::createBodies()
{
    b2World* world = getWorld();

    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    ValueMap frontWheelShape = bodies.at("frontWheelShape").asValueMap();
    ValueMap backWheelShape = bodies.at("backWheelShape").asValueMap();
    ValueMap seatShape = bodies.at("seatShape").asValueMap();
    ValueMap baseShape = bodies.at("baseShape").asValueMap();
    ValueMap shaftShape = bodies.at("shaftShape").asValueMap();
    ValueMap handleShape = bodies.at("handleShape").asValueMap();
    ValueMap cartShape = bodies.at("cartShape").asValueMap();
    ValueMap rear = bodies.at("rear").asValueMap();
    ValueMap front = bodies.at("front").asValueMap();

    b2PolygonShape box;
    b2CircleShape circle;

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position.Set(_origin.x, _origin.y);

    b2FixtureDef fixtureDef;
    fixtureDef.userData = this;
    fixtureDef.friction = 0.3f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 4.0f;
    fixtureDef.filter.categoryBits = 0x201;
    fixtureDef.filter.groupIndex = -2;

    // The frame.
    _frameBody = world->CreateBody(&bodyDef);
    _mainFixture1 = createFixture(_frameBody, fixtureDef, &front, true, true);
    _mainFixture2 = createFixture(_frameBody, fixtureDef, &baseShape, true, true);
    _cartFixture = createFixture(_frameBody, fixtureDef, &cartShape, true, true);
    _handleFixture = createFixture(_frameBody, fixtureDef, &handleShape, true, true);
    fixtureDef.filter.categoryBits = _zeroFilter.categoryBits;
    fixtureDef.filter.maskBits = _zeroFilter.maskBits;
    fixtureDef.filter.groupIndex = -2;
    _mainFixture3 = createFixture(_frameBody, fixtureDef, &seatShape, true, true);
    fixtureDef.filter.categoryBits = 0x201;
    fixtureDef.filter.maskBits = 0xffff;
    fixtureDef.filter.groupIndex = -2;
    _mainFixture4 = createFixture(_frameBody, fixtureDef, &rear, true, true);
    // The shaft overwrites the rear fixture (_shaftFixture stays null; as in the original).
    _mainFixture4 = createFixture(_frameBody, fixtureDef, &shaftShape, true, true);

    // The wheels.
    fixtureDef.density = 5.0f;
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.3f;
    fixtureDef.filter.categoryBits = 0x201;
    fixtureDef.filter.groupIndex = -2;

    Vec2 frontWheelPosition = PointFromString(frontWheelShape.at("pos").asString());
    circle.m_radius = frontWheelShape.at("radius").asFloat();
    fixtureDef.shape = &circle;
    Vec2 position = frontWheelPosition + _origin;
    bodyDef.position.Set(position.x, position.y);
    _frontWheelBody = world->CreateBody(&bodyDef);
    _frontWheelFixture = _frontWheelBody->CreateFixture(&fixtureDef);

    Vec2 backWheelPosition = PointFromString(backWheelShape.at("pos").asString());
    circle.m_radius = backWheelShape.at("radius").asFloat();
    fixtureDef.shape = &circle;
    position = backWheelPosition + _origin;
    bodyDef.position.Set(position.x, position.y);
    _backWheelBody = world->CreateBody(&bodyDef);
    _backWheelFixture = _backWheelBody->CreateFixture(&fixtureDef);

    // The shocks (prismatic to the frame in createJoints, the wheels turn on them).
    bodyDef.position = _backWheelBody->GetPosition();
    box.SetAsBox(0.096f, 0.096f);
    _backShockBody = world->CreateBody(&bodyDef);
    fixtureDef.shape = &box;
    _backShockBody->CreateFixture(&fixtureDef);

    bodyDef.position = _frontWheelBody->GetPosition();
    box.SetAsBox(0.096f, 0.096f);
    _frontShockBody = world->CreateBody(&bodyDef);
    fixtureDef.shape = &box;
    _frontShockBody->CreateFixture(&fixtureDef);

    // The groceries in the basket: box0..box9; 3 is the soda, 4 the cracker box, 7-8-9 hang
    // together on revolute joints.
    fixtureDef.filter = _zeroFilter;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 0.25f;

    Node* vehicleBackground = getSession()->getVehicleBackground();
    b2Body* box7Body = nullptr;
    b2Body* box8Body = nullptr;
    b2Body* box9Body = nullptr;
    for (unsigned int i = 0; i < 10; i++) {
        std::string key = "box" + patch::to_string(i);
        ValueMap boxShape = bodies.at(key).asValueMap();
        Vec2 boxPosition = PointFromString(boxShape.at("pos").asString());
        bodyDef.angle = boxShape.at("rot").asFloat();
        position = boxPosition + _origin;
        bodyDef.position.Set(position.x, position.y);
        b2Body* body = world->CreateBody(&bodyDef);
        b2Fixture* fixture = createFixture(body, fixtureDef, &boxShape, false, false);

        Sprite* sprite =
            Sprite::createWithSpriteFrameName("motor_cart_box_" + patch::to_string(i) + ".png");
        vehicleBackground->addChild(sprite);
        body->SetUserData(sprite);

        if (i == 3) {
            _sodaFixture = fixture;
            addToPostSolve(fixture);
        } else if (i == 4) {
            _crackerFixture = fixture;
            addToPostSolve(fixture);
        }
        getLevel()->addToPaintBody(body);

        if (i == 7) {
            box7Body = body;
        } else if (i == 8) {
            box8Body = body;
        } else if (i == 9) {
            box9Body = body;
        }
    }

    _smashedSodaSprite = Sprite::createWithSpriteFrameName("motor_cart_box_3_2.png");
    _smashedSodaSprite->setVisible(false);
    vehicleBackground->addChild(_smashedSodaSprite);

    // Both anchors use box 7's shape (top edge), as in the original.
    b2RevoluteJointDef jointDef;
    b2PolygonShape* box7Shape = static_cast<b2PolygonShape*>(box7Body->GetFixtureList()->GetShape());
    jointDef.Initialize(box7Body, box8Body,
                        box7Body->GetWorldPoint(b2Vec2(0.0f, box7Shape->m_vertices[2].y)));
    world->CreateJoint(&jointDef);
    jointDef.Initialize(box8Body, box9Body,
                        box8Body->GetWorldPoint(b2Vec2(0.0f, box7Shape->m_vertices[2].y)));
    world->CreateJoint(&jointDef);

    addToPostSolve(_mainFixture1);
    addToPostSolve(_mainFixture2);
    addToPostSolve(_mainFixture3);
    addToPostSolve(_mainFixture4);
    addToPostSolve(_cartFixture);
    addToBeginContact(_mainFixture1);
    addToBeginContact(_mainFixture2);
    addToBeginContact(_mainFixture3);
    addToBeginContact(_mainFixture4);
    addToBeginContact(_frontWheelFixture);
    addToBeginContact(_backWheelFixture);
    addToEndContact(_frontWheelFixture);
    addToEndContact(_backWheelFixture);

    _frameBody->ResetMassData();
    _frontWheelBody->ResetMassData();
    _backWheelBody->ResetMassData();
    _backShockBody->ResetMassData();
    _frontShockBody->ResetMassData();

    _frameBody->SetUserData(_frameSprite);
    _frontWheelBody->SetUserData(_frontWheelSprite);
    _backWheelBody->SetUserData(_backWheelSprite);
    getLevel()->addToPaintBody(_frameBody);
    getLevel()->addToPaintBody(_frontWheelBody);
    getLevel()->addToPaintBody(_backWheelBody);
}

// @005f6994
void MotorCart::addCharacter(CharacterB2D* character)
{
    b2World* world = getWorld();
    Vehicle::addCharacter(character);

    float neckAngle = character->getHeadBody()->GetAngle() - character->getChestBody()->GetAngle();
    character->getNeckJoint()->SetLimits(-0.17453292f - neckAngle, 0.17453292f - neckAngle);

    // Seat: pelvis and thighs pinned to the frame where they are.
    b2RevoluteJointDef jointDef;
    b2Vec2 anchor = character->getPelvisBody()->GetPosition();
    jointDef.Initialize(_frameBody, character->getPelvisBody(), anchor);
    b2Joint* joint = world->CreateJoint(&jointDef);
    _bodyVehicleJointDict[character->getPelvisBody()] = joint;

    anchor = character->getUpperLeg1Body()->GetPosition();
    jointDef.Initialize(_frameBody, character->getUpperLeg1Body(), anchor);
    joint = world->CreateJoint(&jointDef);
    _bodyVehicleJointDict[character->getUpperLeg1Body()] = joint;

    anchor = character->getUpperLeg2Body()->GetPosition();
    jointDef.Initialize(_frameBody, character->getUpperLeg2Body(), anchor);
    joint = world->CreateJoint(&jointDef);
    _bodyVehicleJointDict[character->getUpperLeg2Body()] = joint;

    // Hands on the handle.
    ValueMap joints = _bodiesDict["joints"].asValueMap();
    Vec2 handleAnchor = PointFromString(joints["handleAnchor"].asString());
    Vec2 handlePosition = handleAnchor + _origin;
    b2Vec2 handlePoint(handlePosition.x, handlePosition.y);

    b2RevoluteJointDef handleJointDef;
    handleJointDef.maxMotorTorque = _maxTorque;
    handleJointDef.Initialize(_frameBody, character->getLowerArm1Body(), handlePoint);
    joint = world->CreateJoint(&handleJointDef);
    _bodyVehicleJointDict[character->getLowerArm1Body()] = joint;

    handleJointDef.Initialize(_frameBody, character->getLowerArm2Body(), handlePoint);
    joint = world->CreateJoint(&handleJointDef);
    _bodyVehicleJointDict[character->getLowerArm2Body()] = joint;
}

// @005f70d4
void MotorCart::createJoints()
{
    b2World* world = getWorld();

    // Suspension: the shocks slide vertically (frame axis) and are locked until a hop.
    b2PrismaticJointDef shockJointDef;
    shockJointDef.enableLimit = true;
    shockJointDef.lowerTranslation = 0.0f;
    shockJointDef.upperTranslation = 0.0f;
    shockJointDef.enableMotor = false;
    shockJointDef.maxMotorForce = 1000.0f;
    shockJointDef.motorSpeed = 0.0f;
    shockJointDef.Initialize(_frameBody, _backShockBody, _backWheelBody->GetPosition(),
                             b2Vec2(0.0f, 1.0f));
    _backShockJoint = static_cast<b2PrismaticJoint*>(world->CreateJoint(&shockJointDef));
    shockJointDef.Initialize(_frameBody, _frontShockBody, _frontWheelBody->GetPosition(),
                             b2Vec2(0.0f, 1.0f));
    _frontShockJoint = static_cast<b2PrismaticJoint*>(world->CreateJoint(&shockJointDef));

    b2RevoluteJointDef wheelJointDef;
    wheelJointDef.maxMotorTorque = _maxTorque;
    wheelJointDef.Initialize(_backShockBody, _backWheelBody, _backWheelBody->GetPosition());
    _backWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&wheelJointDef));
    wheelJointDef.Initialize(_frontShockBody, _frontWheelBody, _frontWheelBody->GetPosition());
    _frontWheelJoint = static_cast<b2RevoluteJoint*>(world->CreateJoint(&wheelJointDef));

    addWheelJoint(_frontWheelJoint, _frontWheelBody);
    addWheelJoint(_backWheelJoint, _backWheelBody);
}

// @005f72a8  (impulse and normal are unused)
void MotorCart::cartSmash(float impulse, b2Vec2 normal, bool useSound)
{
    removePostSolve(_cartFixture);
    _frameBody->DestroyFixture(_cartFixture);
    _frameBody->ResetMassData();
    _cartSprite->removeFromParentAndCleanup(false);
    _cartSprite = nullptr;
    _cartFixture = nullptr;

    b2Vec2 burstPoint = _frameBody->GetWorldPoint(_frameBody->GetLocalCenter() + b2Vec2(0.56f, 0.24f));
    float ptm = getPtm();
    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        Emitter* burst = BurstEmitter::createCartBurst(Vec2(burstPoint.x * ptm, burstPoint.y * ptm));
        if (burst) {
            particles->addChild(burst);
        }
    }

    if (useSound) {
        createBodySound("MetalSmashLight", _frameBody, 1.0f, false);
    }
}

// @005f7450  (the frame breaks into five loose parts; impulse and normal are unused)
void MotorCart::frameSmash(float impulse, b2Vec2 normal)
{
    _contactResultBufferDict.erase(_mainFixture1);
    removePostSolve(_mainFixture1);
    removePostSolve(_mainFixture2);
    removePostSolve(_mainFixture3);
    removePostSolve(_mainFixture4);
    _vehicleSmashed = true;

    b2World* world = getWorld();
    ejectAllCharacters();
    if (_cartFixture) {
        // RE-TODO(@005f759c): the binary passes no impulse/normal (dead arguments); iOS passed
        // (100, 0). cartSmash ignores them.
        cartSmash(impulse, normal, false);
    }
    getLevel()->removeFromPaintBody(_frameBody);
    static_cast<Node*>(_frameBody->GetUserData())->removeFromParentAndCleanup(false);
    world->DestroyJoint(_frontWheelJoint);
    world->DestroyJoint(_backWheelJoint);

    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    ValueMap seatShape = bodies.at("seatShape").asValueMap();
    ValueMap baseShape = bodies.at("baseShape").asValueMap();
    ValueMap shaftShape = bodies.at("shaftShape").asValueMap();
    ValueMap handleShape = bodies.at("handleShape").asValueMap();
    ValueMap cartShape = bodies.at("cartShape").asValueMap();  // unused
    ValueMap rear = bodies.at("rear").asValueMap();
    ValueMap front = bodies.at("front").asValueMap();

    Node* vehicleForeground = getSession()->getVehicleForeground();
    Sprite* frameBaseSprite = Sprite::createWithSpriteFrameName("motor_cart_frameBase.png");
    Sprite* frameFrontSprite = Sprite::createWithSpriteFrameName("motor_cart_frameFront.png");
    frameFrontSprite->setAnchorPoint(Vec2(-1.4f, 1.995f));
    Sprite* frameRearSprite = Sprite::createWithSpriteFrameName("motor_cart_frameRear.png");
    frameRearSprite->setAnchorPoint(Vec2(0.555f, 1.79f));
    Sprite* frameSeatSprite = Sprite::createWithSpriteFrameName("motor_cart_frameSeat.png");
    _handleSprite = Sprite::createWithSpriteFrameName("motor_cart_handle.png");
    _handleSprite->setAnchorPoint(Vec2(-1.335f, 0.97f));
    vehicleForeground->addChild(frameBaseSprite);
    vehicleForeground->addChild(frameFrontSprite);
    vehicleForeground->addChild(frameRearSprite);
    vehicleForeground->addChild(frameSeatSprite);
    vehicleForeground->addChild(_handleSprite);

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = _frameBody->GetPosition();
    bodyDef.angle = _frameBody->GetAngle();
    b2Vec2 linearVelocity = _frameBody->GetLinearVelocity();
    float angularVelocity = _frameBody->GetAngularVelocity();

    b2FixtureDef fixtureDef;
    fixtureDef.friction = _mainFixture1->GetFriction();
    fixtureDef.restitution = _mainFixture1->GetRestitution();
    fixtureDef.density = _mainFixture1->GetDensity();
    fixtureDef.filter = _zeroFilter;

    world->DestroyBody(_frameBody);
    world->DestroyBody(_frontShockBody);
    world->DestroyBody(_backShockBody);
    _backShockBody = nullptr;
    _frontShockBody = nullptr;
    _frameBody = nullptr;

    bodyDef.userData = frameFrontSprite;
    b2Body* body = world->CreateBody(&bodyDef);
    createFixture(body, fixtureDef, &front, false, false);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    bodyDef.userData = frameBaseSprite;
    body = world->CreateBody(&bodyDef);
    createFixture(body, fixtureDef, &baseShape, false, true);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    bodyDef.userData = frameRearSprite;
    body = world->CreateBody(&bodyDef);
    createFixture(body, fixtureDef, &rear, false, true);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    bodyDef.userData = frameSeatSprite;
    body = world->CreateBody(&bodyDef);
    createFixture(body, fixtureDef, &seatShape, false, true);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    bodyDef.userData = _handleSprite;
    body = world->CreateBody(&bodyDef);
    createFixture(body, fixtureDef, &handleShape, true, true);
    createFixture(body, fixtureDef, &shaftShape, true, true);
    body->ResetMassData();
    getLevel()->addToPaintBody(body);
    body->SetLinearVelocity(linearVelocity);
    body->SetAngularVelocity(angularVelocity);

    _frontWheelFixture->SetFilterData(_zeroFilter);
    _backWheelFixture->SetFilterData(_zeroFilter);
    createBodySound("MetalSmashHeavy", body, 1.0f, false);
    _frontShockSprite->removeFromParentAndCleanup(false);
    _backShockSprite->removeFromParentAndCleanup(false);
}

// @005f84fc  (impulse and normal are unused; _crackerFixture is left dangling)
void MotorCart::crackerSmash(float impulse, b2Vec2 normal)
{
    b2Fixture* fixture = _crackerFixture;
    b2Body* body = fixture->GetBody();
    _contactResultBufferDict.erase(fixture);
    removePostSolve(fixture);
    static_cast<Node*>(body->GetUserData())->removeFromParentAndCleanup(false);
    getLevel()->removeFromPaintBody(body);

    b2Vec2 center = body->GetWorldCenter();
    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        Emitter* burst = BurstEmitter::createCrackerBurst(body, Vec2::ZERO);
        if (burst) {
            particles->addChild(burst);
        }
    }
    getWorld()->DestroyBody(body);
    createPositionSound("CrackerSmash", Vec2(center.x, center.y), 1.0f, false);
}

// @005f86fc  (impulse and normal are unused)
void MotorCart::colaSmash(float impulse, b2Vec2 normal)
{
    b2Fixture* fixture = _sodaFixture;
    b2Body* body = fixture->GetBody();
    _contactResultBufferDict.erase(fixture);
    removePostSolve(fixture);
    static_cast<Node*>(body->GetUserData())->removeFromParentAndCleanup(false);
    _smashedSodaSprite->setVisible(true);
    body->SetUserData(_smashedSodaSprite);

    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        Emitter* flow =
            FlowEmitter::createSodaFlow(2.5f, 4.0f, 300, body, b2Vec2(0.0f, 0.16f), 90.0f);
        if (flow) {
            particles->addChildEmitter(flow);
        }
    }
}

// @005f8860  (debug hook: smash the frame)
void MotorCart::debugFunction(int value)
{
    // RE-TODO(@005f8860): the binary tail-calls frameSmash without setting its (unused) float
    // arguments; the values passed here are a guess.
    frameSmash(0.0f, b2Vec2(0.0f, 0.0f));
}

// @005f8864  (hop: drive the shocks down to -_mcJumpTrans, then back up and lock them)
void MotorCart::special1ButtonPressed()
{
    if (_backShockJoint->IsMotorEnabled()) {
        float motorSpeed = _backShockJoint->GetMotorSpeed();
        if (motorSpeed < 0.0f) {
            if (_backShockJoint->GetJointTranslation() < -_mcJumpTrans) {
                _backShockJoint->SetMotorSpeed(1.0f);
                _frontShockJoint->SetMotorSpeed(1.0f);
            }
        } else if (motorSpeed > 0.0f && _backShockJoint->GetJointTranslation() > 0.0f) {
            _backShockJoint->EnableMotor(false);
            _frontShockJoint->EnableMotor(false);
            _backShockJoint->SetLimits(0.0f, 0.0f);
            _frontShockJoint->SetLimits(0.0f, 0.0f);
            _backShockJoint->SetMotorSpeed(0.0f);
            _frontShockJoint->SetMotorSpeed(0.0f);
        }
    } else {
        _backShockJoint->SetMotorSpeed(-5.0f);
        _frontShockJoint->SetMotorSpeed(-5.0f);
        _backShockJoint->SetLimits(-_mcJumpTrans, 0.0f);
        _frontShockJoint->SetLimits(-_mcJumpTrans, 0.0f);
        _backShockJoint->EnableMotor(true);
        _frontShockJoint->EnableMotor(true);
        createBodySound("SegwayJump", _backWheelBody, 1.0f, false);
    }
}

// @005f8a50
void MotorCart::forwardButtonPressed()
{
    Vehicle::forwardButtonPressed();
    addMotorSound();
}

// @005f8a74
void MotorCart::addMotorSound()
{
    if (_motorSound == nullptr) {
        _motorSound = createBodySound("ElectricMotor1", _frontWheelBody, 1.0f, true);
        if (_motorSound) {
            _motorSound->setMaxVolume(0.0f);
            _motorSound->fadeTo(0.35f, _soundFadeTime, false);
        }
    } else {
        _motorSound->fadeTo(0.35f, _soundFadeTime, false);
    }
}

// @005f8b9c
void MotorCart::backButtonPressed()
{
    Vehicle::backButtonPressed();
    addMotorSound();
}

// @005f8bc0
void MotorCart::stopMotorSound()
{
    if (_motorSound) {
        _motorSound->fadeTo(0.0f, _soundFadeTime, true);
        _motorSound = nullptr;
    }
}

// @005f8bf8
void MotorCart::forwardBackButtonsNull()
{
    Vehicle::forwardBackButtonsNull();
    stopMotorSound();
}

// @005f8c34
void MotorCart::leanBackButtonPressed()
{
    setCurrentPose(VehiclePoseLeanBack);

    float angularVelocity = _frameBody->GetAngularVelocity();
    float angle = _frameBody->GetAngle();
    float spin = fmin(fmax((angularVelocity - _maxSpinAV) / -_maxSpinAV, 0.0), 1.0);
    float magnitude = _impulseMagnitudeMax * s_timeStepOverFlashTimeStep;

    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    _frameBody->ApplyLinearImpulse(
        b2Vec2(sinf(angle) * magnitude * spin, -cosf(angle) * magnitude * spin),
        _frameBody->GetWorldPoint(b2Vec2(localCenter.x - 1.0f, localCenter.y)), true);
    _frameBody->ApplyLinearImpulse(
        b2Vec2(sin(angle + M_PI) * magnitude * spin, -cos(angle + M_PI) * magnitude * spin),
        _frameBody->GetWorldPoint(b2Vec2(localCenter.x + 1.0f, localCenter.y)), true);
}

// @005f8e50
void MotorCart::leanForwardButtonPressed()
{
    setCurrentPose(VehiclePoseLeanForward);

    float angularVelocity = _frameBody->GetAngularVelocity();
    float angle = _frameBody->GetAngle();
    float spin = std::min(std::max((angularVelocity + _maxSpinAV) / _maxSpinAV, 0.0f), 1.0f);
    float magnitude = _impulseMagnitudeMax * s_timeStepOverFlashTimeStep;

    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    _frameBody->ApplyLinearImpulse(
        b2Vec2(sinf(angle) * magnitude * spin, -cosf(angle) * magnitude * spin),
        _frameBody->GetWorldPoint(b2Vec2(localCenter.x + 1.0f, localCenter.y)), true);
    _frameBody->ApplyLinearImpulse(
        b2Vec2(sin(angle + M_PI) * magnitude * spin, -cos(angle + M_PI) * magnitude * spin),
        _frameBody->GetWorldPoint(b2Vec2(localCenter.x - 1.0f, localCenter.y)), true);
}

// @005f9064  (same as the motor-enabled half of special1ButtonPressed)
void MotorCart::special1ButtonNull()
{
    if (!_backShockJoint->IsMotorEnabled()) {
        return;
    }
    float motorSpeed = _backShockJoint->GetMotorSpeed();
    if (motorSpeed < 0.0f) {
        if (_backShockJoint->GetJointTranslation() < -_mcJumpTrans) {
            _backShockJoint->SetMotorSpeed(1.0f);
            _frontShockJoint->SetMotorSpeed(1.0f);
        }
    } else if (motorSpeed > 0.0f && _backShockJoint->GetJointTranslation() > 0.0f) {
        _backShockJoint->EnableMotor(false);
        _frontShockJoint->EnableMotor(false);
        _backShockJoint->SetLimits(0.0f, 0.0f);
        _frontShockJoint->SetLimits(0.0f, 0.0f);
        _backShockJoint->SetMotorSpeed(0.0f);
        _frontShockJoint->SetMotorSpeed(0.0f);
    }
}

// @005f9144
void MotorCart::ejectBtnPressed()
{
    CharacterB2D* character = _characters[0];
    float angle = _frameBody->GetAngle() + M_PI_2;
    b2Vec2 impulse(cosf(angle) * 4.0f, sinf(angle) * 4.0f);
    character->getChestBody()->ApplyLinearImpulse(
        impulse, character->getChestBody()->GetWorldCenter(), true);
    character->getPelvisBody()->ApplyLinearImpulse(
        impulse, character->getPelvisBody()->GetWorldCenter(), true);
    Vehicle::ejectBtnPressed();
}

// @005f92b4
bool MotorCart::ejectCharacter(CharacterB2D* character)
{
    bool wasEjected = _ejected;
    if (!wasEjected) {
        _ejected = true;
        forwardBackButtonsNull();
        leanButtonsNull();
        special1ButtonNull();
        Vehicle::ejectCharacter(character);

        b2Filter filter = _zeroFilter;
        filter.groupIndex = -2;
        // QOL (PC addition): the binary reads _characters[0] after the rider was erased (its only
        // rider, still in the vector's storage); the same character without reading past the end.
        CharacterB2D* driver = character;
        _frontWheelBody->GetFixtureList()->SetFilterData(filter);
        _backWheelBody->GetFixtureList()->SetFilterData(filter);

        if (!_vehicleSmashed) {
            for (b2Fixture* fixture = _frameBody->GetFixtureList(); fixture;
                 fixture = fixture->GetNext()) {
                fixture->SetFilterData(filter);
            }
            _backShockJoint->EnableMotor(false);
            _frontShockJoint->EnableMotor(false);
            _backShockJoint->SetLimits(0.0f, 0.0f);
            _frontShockJoint->SetLimits(0.0f, 0.0f);
            _backShockJoint->SetMotorSpeed(0.0f);
            _frontShockJoint->SetMotorSpeed(0.0f);

            // Without a rider the frame (and its handle) moves in front of the character layer.
            // _frameSprite keeps pointing at the removed sprite.
            int zOrder =
                static_cast<Node*>(driver->getLowerArm1Body()->GetUserData())->getLocalZOrder();
            static_cast<Node*>(_frameBody->GetUserData())->removeFromParentAndCleanup(false);
            Sprite* frameSprite = Sprite::createWithSpriteFrameName("motor_cart_frame.png");
            frameSprite->setAnchorPoint(_frameAnchor);
            getSession()->getVehicleForeground()->addChild(frameSprite, zOrder);
            addHandleToFrameSprite(frameSprite);
            _frameBody->SetUserData(frameSprite);
        }

        // Push the rider off; the impulse points are the first rider's bodies (the same character,
        // see above).
        float angle = _frameBody->GetAngle() + M_PI_2;
        b2Vec2 impulse(cosf(angle) * 4.0f, sinf(angle) * 4.0f);
        character->getChestBody()->ApplyLinearImpulse(
            impulse, driver->getChestBody()->GetWorldCenter(), true);
        character->getPelvisBody()->ApplyLinearImpulse(
            impulse, driver->getPelvisBody()->GetWorldCenter(), true);
    }
    return !wasEjected;
}

// @005f9634
void MotorCart::checkStateOfCharacter(CharacterB2D* character)
{
    int freeLimbs = 0;
    b2Joint* upperLeg1Joint = _bodyVehicleJointDict[character->getUpperLeg1Body()];
    b2Joint* upperLeg2Joint = _bodyVehicleJointDict[character->getUpperLeg2Body()];
    if (upperLeg1Joint == nullptr) {
        freeLimbs++;
    }
    if (upperLeg2Joint == nullptr) {
        freeLimbs++;
    }

    // Both legs off the seat: release the pelvis, the cart stops colliding with its rider.
    if (upperLeg1Joint == nullptr && upperLeg2Joint == nullptr &&
        _bodyVehicleJointDict[character->getPelvisBody()] != nullptr) {
        b2Filter filter = _zeroFilter;
        filter.groupIndex = -2;
        _frontWheelBody->GetFixtureList()->SetFilterData(filter);
        _backWheelBody->GetFixtureList()->SetFilterData(filter);
        for (b2Fixture* fixture = _frameBody->GetFixtureList(); fixture;
             fixture = fixture->GetNext()) {
            fixture->SetFilterData(filter);
        }
        b2Joint* pelvisJoint = _bodyVehicleJointDict[character->getPelvisBody()];
        getWorld()->DestroyJoint(pelvisJoint);
        _bodyVehicleJointDict.erase(character->getPelvisBody());
    }

    if (_bodyVehicleJointDict[character->getLowerArm1Body()] == nullptr) {
        freeLimbs++;
    }
    if (_bodyVehicleJointDict[character->getLowerArm2Body()] == nullptr) {
        freeLimbs++;
    }
    if (freeLimbs > 2) {
        ejectCharacter(character);
    }
}

// @005f9bb8
void MotorCart::actions()
{
    Vehicle::actions();
}

// @005f9bbc
void MotorCart::paint()
{
    if (_vehicleSmashed) {
        return;
    }

    // Shock sprites: centred between the joint anchors, stretched with the joint travel.
    b2Vec2 shockPosition = 0.5f * (_frontShockJoint->GetAnchorB() - _frontShockJoint->GetAnchorA()) +
                           _frontShockJoint->GetAnchorA();
    float shockScale = fabsf(_frontShockJoint->GetJointTranslation()) / 0.0484375f;
    float rotation = -CC_RADIANS_TO_DEGREES(_frameBody->GetAngle());
    _frontShockSprite->setPosition(Vec2(shockPosition.x * getPtm(), shockPosition.y * getPtm()));
    _frontShockSprite->setScaleY(shockScale);
    _frontShockSprite->setRotation(rotation);

    shockPosition = 0.5f * (_backShockJoint->GetAnchorB() - _backShockJoint->GetAnchorA()) +
                    _backShockJoint->GetAnchorA();
    shockScale = fabsf(_backShockJoint->GetJointTranslation()) / 0.0484375f;
    _backShockSprite->setPosition(Vec2(shockPosition.x * getPtm(), shockPosition.y * getPtm()));
    _backShockSprite->setScaleY(shockScale);
    _backShockSprite->setRotation(rotation);

    if (_cartSprite) {
        b2Vec2 center = _frameBody->GetWorldCenter();
        _cartSprite->setPosition(Vec2(center.x * getPtm(), center.y * getPtm()));
        _cartSprite->setRotation(rotation);
    }
}

// @005f9e24
void MotorCart::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (fixture == _mainFixture1 || fixture == _mainFixture2 || fixture == _mainFixture3 ||
        fixture == _mainFixture4) {
        contactSoundHandler(fixture, otherFixture, contact, _mainFixture1);
        return;
    }
    if (fixture == _cartFixture) {
        contactSoundHandler(fixture, otherFixture, contact, nullptr);
        return;
    }
    Vehicle::beginContact(fixture, otherFixture, contact);
}

// @005f9e78  (records only the impulse; frame hits strong enough for the frame limit are filed
// under _mainFixture1)
void MotorCart::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                          const b2ContactImpulse* impulse)
{
    float normalImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        normalImpulse = b2Max(normalImpulse, impulse->normalImpulses[1]);
    }

    if (fixture == _mainFixture1 || fixture == _mainFixture2 || fixture == _mainFixture3 ||
        fixture == _mainFixture4) {
        if (normalImpulse > _contactImpulseDict[_mainFixture1]) {
            _contactResultBufferDict[_mainFixture1].impulse = normalImpulse;
            return;
        }
    }
    if (normalImpulse > _contactImpulseDict[fixture]) {
        _contactResultBufferDict[fixture].impulse = normalImpulse;
    }
}

// @005fa11c
void MotorCart::handleContactResults()
{
    if (_contactResultBufferDict[_mainFixture1].impulse != 0.0f) {
        VehicleContact contact = _contactResultBufferDict[_mainFixture1];
        frameSmash(contact.impulse, contact.normal);
    }
    if (_contactResultBufferDict[_cartFixture].impulse != 0.0f) {
        VehicleContact contact = _contactResultBufferDict[_cartFixture];
        cartSmash(contact.impulse, contact.normal, true);
    }
    if (_contactResultBufferDict[_sodaFixture].impulse != 0.0f) {
        VehicleContact contact = _contactResultBufferDict[_sodaFixture];
        colaSmash(contact.impulse, contact.normal);
    }
    if (_contactResultBufferDict[_crackerFixture].impulse != 0.0f) {
        VehicleContact contact = _contactResultBufferDict[_crackerFixture];
        crackerSmash(contact.impulse, contact.normal);
    }
    _contactResultBufferDict.clear();
}

// ---------------------------------------------------------------------------------------------
// QOL (PC addition): re-grab vehicle (Vehicle.h). Not in the original.
// ---------------------------------------------------------------------------------------------

b2Body* MotorCart::qolFrameBody()
{
    return _frameBody;
}

// checkStateOfCharacter: off once three of his thighs and hands have let go.
bool MotorCart::qolCanRemount(CharacterB2D* character)
{
    if (_vehicleSmashed || !_frameBody) {
        return false;
    }
    int lost = (character->qolLostUpperLeg(1) ? 1 : 0) + (character->qolLostUpperLeg(2) ? 1 : 0) +
               (character->qolLostLowerArm(1) ? 1 : 0) + (character->qolLostLowerArm(2) ? 1 : 0);
    return lost <= 2;
}

// ejectCharacter: _ejected, controls nulled, wheels and frame out of the rider's group
// (qolRestoreFilters), the shocks locked (as while riding), and the frame sprite moved in front of
// the characters - it goes back behind them, as createSprites put it.
void MotorCart::qolRemount(CharacterB2D* character)
{
    _ejected = false;
    qolRestoreFilters(character);
    Node* current = static_cast<Node*>(_frameBody->GetUserData());
    if (current) {
        current->removeFromParentAndCleanup(false);
    }
    _frameSprite = Sprite::createWithSpriteFrameName("motor_cart_frame.png");
    _frameSprite->setAnchorPoint(_frameAnchor);
    getSession()->getVehicleBackground()->addChild(_frameSprite);
    addHandleToFrameSprite(_frameSprite);
    _frameBody->SetUserData(_frameSprite);
    qolMount(character, [this, character]() { addCharacter(character); });
    qolReplayInjuries(character);
}
