#include "PogoStick.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "CharacterB2D.h"
#include "Globals.h"
#include "LevelB2D.h"
#include "Session.h"
#include "Sound.h"

USING_NS_CC;

// File statics of the original translation unit (dynamic initialisation, _INIT_17 @00608358): the
// spring travel of the pogo joint in metres. Float math on the exported Flash ptm ratio.
// Hop: the spring is opened to this translation before a forward/back hop is released.
static float s_hopTranslation = (30.0f / globals::flash::ptmRatio) * 0.5f;     // @00ac6534
// Charge: special1 compresses the spring up to this translation; also the 100 % mark of the
// release sound volume and of the spring sprite's squash.
static float s_chargeTranslation = (40.0f / globals::flash::ptmRatio) * 0.5f;  // @00ac6538

// @0060304c
PogoStick::PogoStick()
    : _leg2Injured(false),
      _leg1Injured(false),
      _impulseMagnitudeMax(0.5f),
      _impulseOffset(1.0f),
      _maxSpinAV(5.0f),
      _nubContacts(0),
      _charging(false),
      _jumpFrames(0),
      _airLean(false),
      _targetAngle(0.0f),
      _frameSmashLimit(40.0f),
      _frameSmashed(false),
      _destroyFrameBody(false),
      _handleAnchorPoint(b2Vec2_zero),
      _footAnchorPoint(b2Vec2_zero),
      // _frameBody (+0x330) is left uninitialised by the original constructor.
      _rodBody(nullptr),
      _frameShape(nullptr),
      _rodShape(nullptr),
      _nubShape(nullptr),
      _stopperShape(nullptr),
      _frameMC(nullptr),
      _rodMC(nullptr),
      _brokenFrame1MC(nullptr),
      _brokenFrame2MC(nullptr),
      _springMC(nullptr),
      _pogoJoint(nullptr),
      _frameHand1(nullptr),
      _frameHand2(nullptr),
      _frameFoot1(nullptr),
      _frameFoot2(nullptr),
      _currCOM(b2Vec2_zero),
      _prevCOM(b2Vec2_zero),
      _velocityCOM(b2Vec2_zero),
      _velocityAngle(0.0f),
      _pivotDirection(b2Vec2_zero),
      _pivotAngle(0.0f),
      _tempElbowBreakLimit(0),
      _tempElbowLigamentLimit(0),
      _tempKneeBreakLimit(0),
      _tempKneeLigamentLimit(0),
      _correctionMultiplier(0.0f),
      _fps(0.0f)
{
}

// @00603264 (D1), @006032b0 (D0)
PogoStick::~PogoStick()
{
}

// @006032d4
bool PogoStick::init(Vec2 position, std::string name, int groupID)
{
    bool result = Vehicle::init(position, name, groupID);
    if (result) {
        _leg2Injured = false;
        _leg1Injured = false;
        _airLean = false;
        _targetAngle = 1.57079637f;
        _currentPose = VehiclePoseExtra1;
        getLevel()->addToPaintItem(this);
        _fps = 1.0f / getTimeStep();

        Node* characterForeground = getSession()->getCharacterForeground();
        DrawNode* drawNode = DrawNode::create(2.0f);
        drawNode->setTag(8765);
        characterForeground->addChild(drawNode, 1000);
    }
    return result;
}

// @00603420
void PogoStick::timeStepChanged(float timeStep)
{
    _fps = 1.0f / timeStep;
}

// @00603430
void PogoStick::createSprites()
{
    Node* vehicleBackground = getSession()->getVehicleBackground();

    _springMC = Sprite::createWithSpriteFrameName("pogo_stick_spring.png");

    if (getSession()->getMode() == SessionModeGameplay) {
        _brokenFrame1MC = Sprite::createWithSpriteFrameName("pogo_stick_brokenframe1.png");
        _brokenFrame2MC = Sprite::createWithSpriteFrameName("pogo_stick_brokenframe2.png");
        vehicleBackground->addChild(_brokenFrame1MC);
        vehicleBackground->addChild(_brokenFrame2MC);
        _brokenFrame1MC->setVisible(false);
        _brokenFrame2MC->setVisible(false);
    }

    _frameMC = Sprite::createWithSpriteFrameName("pogo_stick_frame.png");
    _rodMC = Sprite::createWithSpriteFrameName("pogo_stick_rod.png");
    _rodMC->setAnchorPoint(Vec2(0.5f, 0.7f));

    _springMC->setPosition(Vec2(_frameMC->getTextureRect().size.width * 0.5f,
                                _frameMC->getTextureRect().size.height));
    _springMC->setAnchorPoint(Vec2(0.5f, 1.0f));

    vehicleBackground->addChild(_rodMC);
    vehicleBackground->addChild(_frameMC);
    _frameMC->addChild(_springMC, -1);
}

// @00603730
void PogoStick::createBodies()
{
    b2World* world = getWorld();

    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    ValueMap rodShape = bodies.at("rodShape").asValueMap();
    ValueMap frameShape = bodies.at("frameShape").asValueMap();
    ValueMap joints = _bodiesDict.at("joints").asValueMap();
    // Read but unused here (addCharacter reads them again).
    std::string handleAnchor = joints.at("handleAnchor").asString();
    std::string footAnchor = joints.at("footAnchor").asString();

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;

    b2PolygonShape polygonShape;
    b2CircleShape circleShape;

    b2FixtureDef fixtureDef;
    fixtureDef.filter = _defaultFilter;
    fixtureDef.density = 5.0f;
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.1f;

    // Frame.
    Vec2 position = PointFromString(frameShape.at("pos").asString());
    Size size = SizeFromString(frameShape.at("size").asString());
    float rotation = frameShape.at("rot").asFloat();
    bodyDef.angle = rotation;
    bodyDef.position = b2Vec2(position.x + _origin.x, position.y + _origin.y);
    polygonShape.SetAsBox(size.width, size.height);
    fixtureDef.shape = &polygonShape;
    _frameBody = world->CreateBody(&bodyDef);
    _frameShape = _frameBody->CreateFixture(&fixtureDef);
    addToBeginContact(_frameShape);
    addToPostSolve(_frameShape);

    // Rod.
    position = PointFromString(rodShape.at("pos").asString());
    size = SizeFromString(rodShape.at("size").asString());
    rotation = rodShape.at("rot").asFloat();
    bodyDef.angle = rotation;
    bodyDef.position = b2Vec2(position.x + _origin.x, position.y + _origin.y);
    polygonShape.SetAsBox(size.width, size.height);
    _rodBody = world->CreateBody(&bodyDef);
    _rodShape = _rodBody->CreateFixture(&fixtureDef);

    // Nub: the ball at the bottom end of the rod.
    fixtureDef.filter = _defaultFilter;
    fixtureDef.density = 5.0f;
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.3f;
    circleShape.m_radius = 0.056f;
    circleShape.m_p = b2Vec2(0.0f, -size.height);
    fixtureDef.shape = &circleShape;
    _nubShape = _rodBody->CreateFixture(&fixtureDef);
    addToBeginContact(_nubShape);
    addToEndContact(_nubShape);

    _rodBody->SetUserData(_rodMC);
    _frameBody->SetUserData(_frameMC);
    getLevel()->addToPaintBody(_frameBody);
    getLevel()->addToPaintBody(_rodBody);
    _frameBody->ResetMassData();
    _rodBody->ResetMassData();
}

// @00604180
void PogoStick::createJoints()
{
    b2PrismaticJointDef jointDef;
    jointDef.enableLimit = true;
    jointDef.enableMotor = true;
    jointDef.maxMotorForce = 10000.0f;

    b2Vec2 anchor = _frameBody->GetPosition();
    float angle = _frameBody->GetAngle();
    b2Vec2 axis(-sinf(angle), cosf(angle));
    jointDef.Initialize(_frameBody, _rodBody, anchor, axis);
    _pogoJoint = static_cast<b2PrismaticJoint*>(getWorld()->CreateJoint(&jointDef));
}

// @00604268
void PogoStick::createDictionaries()
{
    _contactImpulseDict[_frameShape] = _frameSmashLimit;
    _contactAddSounds[_frameShape] = "Thud1";
}

// @006043e0
void PogoStick::addCharacter(CharacterB2D* character)
{
    Vehicle::addCharacter(character);
    b2World* world = getWorld();

    // Limit the rider's joints around the riding pose (relative to the current angles).
    float angle = character->getUpperLeg1Body()->GetAngle() - character->getPelvisBody()->GetAngle();
    character->getHipJoint1()->SetLimits(-angle, 1.74532926f - angle);
    angle = character->getUpperLeg2Body()->GetAngle() - character->getPelvisBody()->GetAngle();
    character->getHipJoint2()->SetLimits(-angle, 1.74532926f - angle);
    angle = character->getUpperArm1Body()->GetAngle() - character->getChestBody()->GetAngle();
    character->getShoulderJoint1()->SetLimits(-angle, 2.09439516f - angle);
    angle = character->getUpperArm2Body()->GetAngle() - character->getChestBody()->GetAngle();
    character->getShoulderJoint2()->SetLimits(-angle, 2.09439516f - angle);
    angle = character->getLowerArm1Body()->GetAngle() - character->getUpperArm1Body()->GetAngle();
    character->getElbowJoint1()->SetLimits(-angle, 1.04719758f - angle);
    angle = character->getLowerArm2Body()->GetAngle() - character->getUpperArm2Body()->GetAngle();
    character->getElbowJoint2()->SetLimits(-angle, 1.04719758f - angle);
    angle = character->getHeadBody()->GetAngle() - character->getChestBody()->GetAngle();
    character->getNeckJoint()->SetLimits(-0.0872664601f - angle, -angle);

    character->scaleJointLimit(character->getElbowJoint1(), 1.5f);
    character->scaleJointLimit(character->getElbowJoint2(), 1.5f);
    character->scaleJointLimit(character->getKneeJoint1(), 1.8461f);
    character->scaleJointLimit(character->getKneeJoint2(), 1.8461f);
    character->multiplyElbowLigamentLimit(1.3924f);
    character->multiplyKneeLigamentLimit(1.75f);

    // Hands to the handles, feet to the pegs.
    b2RevoluteJointDef jointDef;
    jointDef.enableLimit = true;
    jointDef.lowerAngle = -0.17453292f;
    jointDef.upperAngle = 1.74532926f;

    ValueMap joints = _bodiesDict.at("joints").asValueMap();
    std::string handleAnchorString = joints.at("handleAnchor").asString();
    std::string footAnchorString = joints.at("footAnchor").asString();

    Vec2 point = PointFromString(handleAnchorString);
    b2Vec2 anchor(point.x + _origin.x, point.y + _origin.y);
    jointDef.Initialize(_frameBody, character->getLowerArm1Body(), anchor);
    _frameHand1 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    _bodyVehicleJointDict[character->getLowerArm1Body()] = _frameHand1;
    _handleAnchorPoint = _frameBody->GetLocalPoint(anchor);
    jointDef.Initialize(_frameBody, character->getLowerArm2Body(), anchor);
    _frameHand2 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    _bodyVehicleJointDict[character->getLowerArm2Body()] = _frameHand2;

    jointDef.lowerAngle = -0.17453292f;
    jointDef.upperAngle = 0.17453292f;
    point = PointFromString(footAnchorString);
    anchor = b2Vec2(point.x + _origin.x, point.y + _origin.y);
    jointDef.Initialize(_frameBody, character->getLowerLeg1Body(), anchor);
    _frameFoot1 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    _bodyVehicleJointDict[character->getLowerLeg1Body()] = _frameFoot1;
    _footAnchorPoint = _frameBody->GetLocalPoint(anchor);
    jointDef.Initialize(_frameBody, character->getLowerLeg2Body(), anchor);
    _frameFoot2 = static_cast<b2RevoluteJoint*>(world->CreateJoint(&jointDef));
    _bodyVehicleJointDict[character->getLowerLeg2Body()] = _frameFoot2;

    // Bodies of the centre of mass used by the lean correction.
    _COMArray.push_back(_frameBody);
    _COMArray.push_back(_rodBody);
    _COMArray.push_back(character->getHeadBody());
    _COMArray.push_back(character->getChestBody());
    _COMArray.push_back(character->getPelvisBody());
    _COMArray.push_back(character->getUpperArm1Body());
    _COMArray.push_back(character->getLowerArm1Body());
    _COMArray.push_back(character->getUpperArm2Body());
    _COMArray.push_back(character->getLowerArm2Body());
    _COMArray.push_back(character->getUpperLeg1Body());
    _COMArray.push_back(character->getLowerLeg1Body());
    _COMArray.push_back(character->getUpperLeg2Body());
    _COMArray.push_back(character->getLowerLeg2Body());
    _prevCOM = getCenterOfMass();
}

// @0060608c
b2Vec2 PogoStick::getCenterOfMass()
{
    b2Vec2 center = b2Vec2_zero;
    float totalMass = 0.0f;
    for (b2Body* body : _COMArray) {
        float mass = body->GetMass();
        totalMass += mass;
        center += mass * body->GetWorldCenter();
    }
    return (1.0f / totalMass) * center;
}

// @00606158
bool PogoStick::ejectCharacter(CharacterB2D* character)
{
    Vehicle::ejectCharacter(character);
    getLevel()->addToSingleActions(this);
    _pogoJoint->SetMotorSpeed(0.0f);
    character->resetJointLimits();
    _currentPose = VehiclePoseNone;
    _frameShape->SetFilterData(_zeroFilter);
    _rodShape->SetFilterData(_zeroFilter);

    // Undo the scaling of addCharacter.
    character->scaleJointLimit(character->getElbowJoint1(), 1.0f / 1.5f);
    character->scaleJointLimit(character->getElbowJoint2(), 1.0f / 1.5f);
    character->scaleJointLimit(character->getKneeJoint1(), 1.0f / 1.8461f);
    character->scaleJointLimit(character->getKneeJoint2(), 1.0f / 1.8461f);
    character->multiplyElbowLigamentLimit(1.0f / 1.3924f);
    character->multiplyKneeLigamentLimit(1.0f / 1.75f);
    return true;
}

// @00606274  (after the rider was ejected: let the spring extend, then lock it)
void PogoStick::singleAction()
{
    float translation = _pogoJoint->GetJointTranslation();
    if (translation > 0.0f) {
        _pogoJoint->SetMotorSpeed(translation * -70.0f);
        return;
    }
    _pogoJoint->SetMotorSpeed(0.0f);
    _pogoJoint->SetLimits(0.0f, 0.0f);
    _jumpFrames = 10;
}

// @006062e0
void PogoStick::leanForwardButtonPressed()
{
    if (_nubContacts == 0) {
        _airLean = true;
    }

    float spin = (_frameBody->GetAngularVelocity() + _maxSpinAV) / _maxSpinAV;
    spin = b2Min(b2Max(spin, 0.0f), 1.0f);
    float angle = _frameBody->GetAngle();
    b2Vec2 impulse = spin * (_impulseMagnitudeMax * s_timeStepOverFlashTimeStep * b2Vec2(cosf(angle), sinf(angle)));

    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x, localCenter.y + _impulseOffset)), true);
    _frameBody->ApplyLinearImpulse(
        -impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x, localCenter.y - _impulseOffset)), true);
}

// @006064a4
void PogoStick::leanBackButtonPressed()
{
    if (_nubContacts == 0) {
        _airLean = true;
    }

    float spin = (_frameBody->GetAngularVelocity() - _maxSpinAV) / -_maxSpinAV;
    spin = b2Clamp(spin, 0.0f, 1.0f);
    float angle = _frameBody->GetAngle();
    b2Vec2 impulse = spin * (_impulseMagnitudeMax * s_timeStepOverFlashTimeStep * b2Vec2(cosf(angle), sinf(angle)));

    b2Vec2 localCenter = _frameBody->GetLocalCenter();
    _frameBody->ApplyLinearImpulse(
        -impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x, localCenter.y + _impulseOffset)), true);
    _frameBody->ApplyLinearImpulse(
        impulse, _frameBody->GetWorldPoint(b2Vec2(localCenter.x, localCenter.y - _impulseOffset)), true);
}

// @00606670  (no lean input: steer the frame toward the target angle, corrected by the motion of
// the centre of mass)
void PogoStick::leanButtonsNull()
{
    updateCOMValues();
    if (_airLean) {
        return;
    }

    float targetAngle;
    if (_velocityCOM.y <= 0.0f) {
        float blend = b2Min(_velocityCOM.y * -8.0f, 1.0f);
        targetAngle = (1.0f - blend) * _targetAngle + blend * (_velocityAngle + M_PI);
    } else {
        float blend = b2Min(_velocityCOM.y * 8.0f, 1.0f);
        targetAngle = blend * _velocityAngle + (1.0f - blend) * _targetAngle;
    }

    float angleDiff = targetAngle - _pivotAngle;
    if (angleDiff > M_PI) {
        angleDiff -= 2.0 * M_PI;
    }
    if (angleDiff < -M_PI) {
        angleDiff += 2.0 * M_PI;
    }

    float angularVelocity = b2Max(_maxSpinAV * -2.0f, angleDiff * 15.0f);
    angularVelocity = b2Min(angularVelocity, _maxSpinAV + _maxSpinAV);
    _frameBody->SetAngularVelocity(angularVelocity);
}

// @006067a8
void PogoStick::updateCOMValues()
{
    _currCOM = getCenterOfMass();
    _velocityCOM = _currCOM - _prevCOM;
    _prevCOM = _currCOM;
    _velocityAngle = atan2f(_velocityCOM.y, _velocityCOM.x);
    _pivotDirection = _currCOM - _rodBody->GetWorldPoint(b2Vec2(0.0f, -0.5f));
    _pivotAngle = atan2f(_pivotDirection.y, _pivotDirection.x);
}

// @006068fc  (hop forward: open the spring, then release it when leaning far enough forward)
void PogoStick::forwardButtonPressed()
{
    _airLean = false;
    _targetAngle = 1.41371667f;
    if (_charging) {
        return;
    }

    float translation = _pogoJoint->GetJointTranslation();
    float motorSpeed = _pogoJoint->GetMotorSpeed();
    if (_nubContacts != 0 && _jumpFrames == 0) {
        if (_pogoJoint->GetUpperLimit() == 0.0f) {
            _pogoJoint->SetLimits(0.0f, s_hopTranslation);
            _pogoJoint->SetMotorSpeed(0.75f);
        } else if (motorSpeed == 0.75f && translation >= s_hopTranslation) {
            if (_pivotAngle < _targetAngle && _pivotAngle > _targetAngle + -0.471238524f) {
                _pogoJoint->SetMotorSpeed(-7.0f);
                _jumpFrames = 10;
                Sound* sound = createBodySound("PogoRelease", _frameBody, 1.0f, false);
                if (sound) {
                    sound->setMaxVolume(b2Min(b2Max(translation / s_chargeTranslation, 0.0f), 1.0f));
                }
            }
        }
    }
    if (translation <= 0.0f && motorSpeed < 0.0f) {
        _pogoJoint->SetLimits(0.0f, 0.0f);
        _pogoJoint->SetMotorSpeed(0.0f);
    }
}

// @00606ad0  (hop backward)
void PogoStick::backButtonPressed()
{
    _airLean = false;
    _targetAngle = 2.10486722f;
    if (_charging) {
        return;
    }

    float translation = _pogoJoint->GetJointTranslation();
    float motorSpeed = _pogoJoint->GetMotorSpeed();
    if (_nubContacts != 0 && _jumpFrames == 0) {
        if (_pogoJoint->GetUpperLimit() == 0.0f) {
            _pogoJoint->SetLimits(0.0f, s_hopTranslation);
            _pogoJoint->SetMotorSpeed(0.75f);
        } else if (motorSpeed == 0.75f && translation >= s_hopTranslation) {
            // Double comparison in the original (unlike forwardButtonPressed).
            if (_pivotAngle > _targetAngle && _pivotAngle < _targetAngle + 0.47123889803846897) {
                _pogoJoint->SetMotorSpeed(-7.0f);
                _jumpFrames = 10;
                Sound* sound = createBodySound("PogoRelease2", _frameBody, 1.0f, false);
                if (sound) {
                    sound->setMaxVolume(b2Min(b2Max(translation / s_chargeTranslation, 0.0f), 1.0f));
                }
            }
        }
    }
    if (translation <= 0.0f && motorSpeed < 0.0f) {
        _pogoJoint->SetLimits(0.0f, 0.0f);
        _pogoJoint->SetMotorSpeed(0.0f);
    }
}

// @00606ca8
void PogoStick::forwardBackButtonsNull()
{
    _targetAngle = 1.57079637f;
    if (_charging) {
        return;
    }

    float translation = _pogoJoint->GetJointTranslation();
    if (_pogoJoint->GetUpperLimit() != 0.0f) {
        if (translation < 0.0f) {
            _pogoJoint->SetMotorSpeed(-0.5f);
            return;
        }
        _pogoJoint->SetMotorSpeed(0.0f);
        _pogoJoint->SetLimits(0.0f, 0.0f);
    }
}

// @00606d40  (charge: compress the spring)
void PogoStick::special1ButtonPressed()
{
    _charging = true;
    if (_pogoJoint->GetUpperLimit() != s_chargeTranslation) {
        _pogoJoint->SetLimits(0.0f, s_chargeTranslation);
    }
    _pogoJoint->SetMotorSpeed(_pogoJoint->GetJointTranslation() >= s_chargeTranslation ? 0.0f : 0.75f);
}

// @00606da8  (release the charge)
void PogoStick::special1ButtonNull()
{
    if (!_charging) {
        return;
    }

    float translation = _pogoJoint->GetJointTranslation();
    if (translation <= 0.0f) {
        _pogoJoint->SetMotorSpeed(0.0f);
        _pogoJoint->SetLimits(0.0f, 0.0f);
        _charging = false;
        _jumpFrames = 10;
        return;
    }

    float motorSpeed = translation * -70.0f;
    if (_pogoJoint->GetMotorSpeed() - motorSpeed > 1.0f) {
        Sound* sound = createBodySound("PogoRelease", _frameBody, 1.0f, false);
        if (sound) {
            sound->setMaxVolume(
                b2Min(b2Max(translation / (s_chargeTranslation * 0.5f), 0.0f), 1.0f));
        }
    }
    _pogoJoint->SetMotorSpeed(motorSpeed);
}

// @00606f24
void PogoStick::checkStateOfCharacter(CharacterB2D* character)
{
    if (_leg1Injured && _leg2Injured) {
        b2Filter filter = _defaultFilter;
        filter.groupIndex = 0;
        _frameShape->SetFilterData(filter);
        _nubShape->SetFilterData(filter);
    }
    if (_bodyVehicleJointDict[character->getLowerArm1Body()] == nullptr &&
        _bodyVehicleJointDict[character->getLowerArm2Body()] == nullptr) {
        character->resetJointLimits();
        ejectCharacter(character);
    }
}

// @00607120
void PogoStick::handleUpperArm1Injury(CharacterB2D* character)
{
    Vehicle::handleUpperArm1Injury(character);
    removeFromCOMArray(character->getUpperArm1Body());
    removeFromCOMArray(character->getLowerArm1Body());
}

// @006071f4
void PogoStick::removeFromCOMArray(b2Body* body)
{
    auto it = std::find(_COMArray.begin(), _COMArray.end(), body);
    if (it != _COMArray.end()) {
        _COMArray.erase(it);
    }
}

// @00607258
void PogoStick::handleUpperArm2Injury(CharacterB2D* character)
{
    Vehicle::handleUpperArm2Injury(character);
    removeFromCOMArray(character->getUpperArm2Body());
    removeFromCOMArray(character->getLowerArm2Body());
}

// @0060732c
void PogoStick::handleLowerArm1Injury(CharacterB2D* character)
{
    Vehicle::handleLowerArm1Injury(character);
    removeFromCOMArray(character->getLowerArm1Body());
}

// @006073a4
void PogoStick::handleLowerArm2Injury(CharacterB2D* character)
{
    Vehicle::handleLowerArm2Injury(character);
    removeFromCOMArray(character->getLowerArm2Body());
}

// @0060741c
void PogoStick::handleUpperLeg1Injury(CharacterB2D* character)
{
    _leg1Injured = true;
    Vehicle::handleUpperLeg1Injury(character);
    removeFromCOMArray(character->getUpperLeg1Body());
    removeFromCOMArray(character->getLowerLeg1Body());
}

// @006074f8
void PogoStick::handleLowerLeg1Injury(CharacterB2D* character)
{
    _leg1Injured = true;
    Vehicle::handleLowerLeg1Injury(character);
    if (character->getKneeJoint1() == nullptr) {
        removeFromCOMArray(character->getLowerLeg1Body());
    }
}

// @00607584
void PogoStick::handleUpperLeg2Injury(CharacterB2D* character)
{
    _leg2Injured = true;
    Vehicle::handleUpperLeg2Injury(character);
    removeFromCOMArray(character->getUpperLeg2Body());
    removeFromCOMArray(character->getLowerLeg2Body());
}

// @00607660
void PogoStick::handleLowerLeg2Injury(CharacterB2D* character)
{
    _leg2Injured = true;
    Vehicle::handleLowerLeg2Injury(character);
    if (character->getKneeJoint2() == nullptr) {
        removeFromCOMArray(character->getLowerLeg2Body());
    }
}

// @006076ec  (a non-sensor at least as heavy as the pogo, or static, touches the nub)
void PogoStick::nubContactAdd(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (otherFixture->IsSensor()) {
        return;
    }
    float otherMass = otherFixture->GetBody()->GetMass();
    if (otherMass == 0.0f || otherMass >= fixture->GetBody()->GetMass()) {
        _airLean = false;
        _nubContacts++;
    }
}

// @0060772c
void PogoStick::handleContactResults()
{
    // The frame's impulse is recorded in LevelItem's buffer (see postSolve), while the map that gets
    // cleared is this class's own one (as in the original).
    if (LevelItem::_contactResultBufferDict[_frameShape].impulse != 0.0f) {
        frameSmash(LevelItem::_contactResultBufferDict[_frameShape].impulse);
    }
    _contactResultBufferDict.clear();
}

// @006078c4  (the frame breaks in two; the spring falls off)
void PogoStick::frameSmash(float impulse)
{
    if (_frameSmashed) {
        return;
    }

    LevelItem::_contactResultBufferDict.erase(_frameShape);
    removePostSolve(_frameShape);
    removeBeginContact(_frameShape);
    ejectAllCharacters();
    b2World* world = getWorld();
    world->DestroyJoint(_pogoJoint);
    _frameSmashed = true;

    Texture2D* texture = _springMC->getTexture();
    Rect textureRect = _springMC->getTextureRect();
    Sprite* springSprite = Sprite::createWithTexture(texture, textureRect, false);
    getSession()->getVehicleForeground()->addChild(springSprite);

    float ptmRatio = globals::flash::ptmRatio;
    b2Vec2 position = _frameBody->GetPosition();
    float angle = _frameBody->GetAngle();
    b2Vec2 linearVelocity = _frameBody->GetLinearVelocity();
    float angularVelocity = _frameBody->GetAngularVelocity();
    b2Vec2 worldCenter = _frameBody->GetWorldCenter();
    b2Transform transform = _frameBody->GetTransform();
    float halfHeight = (31.25f / ptmRatio) * 0.5f;
    float halfWidth = (10.0f / ptmRatio) * 0.5f;

    getLevel()->removeFromPaintBody(_frameBody);
    world->DestroyBody(_frameBody);
    _frameBody = nullptr;
    _frameMC->removeFromParentAndCleanup(false);

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.angle = angle;

    b2PolygonShape shape;
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.density = 5.0f;
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.filter = _zeroFilter;

    // Upper half of the frame.
    b2Body* body = world->CreateBody(&bodyDef);
    b2Vec2 center(0.0f, halfHeight);
    shape.SetAsBox(halfWidth, halfHeight, center, 0.0f);
    body->CreateFixture(&fixtureDef);
    body->ResetMassData();
    body->SetAngularVelocity(angularVelocity);
    body->SetLinearVelocity(linearVelocity +
                            b2Cross(angularVelocity, b2Mul(transform, center) - worldCenter));
    body->SetUserData(_brokenFrame1MC);
    getLevel()->addToPaintBody(body);

    // Lower half.
    body = world->CreateBody(&bodyDef);
    center = b2Vec2(0.0f, -halfHeight);
    shape.SetAsBox(halfWidth, halfHeight, center, 0.0f);
    body->CreateFixture(&fixtureDef);
    body->ResetMassData();
    body->SetAngularVelocity(angularVelocity);
    body->SetLinearVelocity(linearVelocity +
                            b2Cross(angularVelocity, b2Mul(transform, center) - worldCenter));
    body->SetUserData(_brokenFrame2MC);
    getLevel()->addToPaintBody(body);

    _brokenFrame1MC->setAnchorPoint(Vec2(0.5f, 0.05f));
    _brokenFrame2MC->setAnchorPoint(Vec2(0.5f, 0.95f));
    _brokenFrame1MC->setVisible(true);
    _brokenFrame2MC->setVisible(true);

    // The loose spring.
    body = world->CreateBody(&bodyDef);
    shape.SetAsBox((4.0f / ptmRatio) * 0.5f, halfHeight);
    body->CreateFixture(&fixtureDef);
    body->ResetMassData();
    body->SetAngularVelocity(angularVelocity);
    body->SetLinearVelocity(linearVelocity);
    body->SetUserData(springSprite);
    getLevel()->addToPaintBody(body);

    createBodySound("PogoFrameSmash", body, 1.0f, false);
}

// @00607f14
void PogoStick::nubContactRemove(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (otherFixture->IsSensor()) {
        return;
    }
    float otherMass = otherFixture->GetBody()->GetMass();
    if (otherMass == 0.0f || otherMass >= fixture->GetBody()->GetMass()) {
        _nubContacts--;
    }
}

// @00607f50  (rider pose: knees bent)
void PogoStick::extraPose1()
{
    CharacterB2D* character = _characters[0];
    if (character->getKneeJoint1()) {
        character->setJoint(character->getKneeJoint1(), -1.2f, 10.0f, 20.0f);
    }
    if (character->getKneeJoint2()) {
        character->setJoint(character->getKneeJoint2(), -1.2f, 10.0f, 20.0f);
    }
}

// @00607fe0
void PogoStick::actions()
{
    if (_jumpFrames != 0) {
        _jumpFrames--;
    }
    Vehicle::actions();
}

// @00607ff4  (squash the spring sprite with the joint compression)
void PogoStick::paint()
{
    if (_frameSmashed) {
        return;
    }
    float compression = _pogoJoint->GetJointTranslation() / s_chargeTranslation;
    compression = b2Max(b2Min(compression, 1.0f), 0.0f);
    _springMC->setScaleY((1.0f - compression) * 0.55f + 0.45f);
}

// @00608074
void PogoStick::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (fixture == _nubShape) {
        nubContactAdd(fixture, otherFixture, contact);
    }
    contactSoundHandler(fixture, otherFixture, contact, nullptr);
}

// @006080c8
void PogoStick::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (fixture == _nubShape) {
        nubContactRemove(fixture, otherFixture, contact);
    }
}

// @00608110  (only the frame fixture is registered)
void PogoStick::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                          const b2ContactImpulse* impulse)
{
    float normalImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        normalImpulse = b2Max(normalImpulse, impulse->normalImpulses[1]);
    }
    if (normalImpulse > _contactImpulseDict[_frameShape]) {
        if (normalImpulse > LevelItem::_contactResultBufferDict[_frameShape].impulse) {
            LevelItem::_contactResultBufferDict[_frameShape].impulse = normalImpulse;
        }
    }
}
