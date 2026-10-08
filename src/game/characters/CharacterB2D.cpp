// CharacterB2D: the ragdoll rider shared by every playable character.
// Reconstructed from libMyGame.so 1.1.3 (arm64); see CharacterB2D.h for the layout notes and
// docs/modules/M1.md for the analysis notes.

#include "CharacterB2D.h"

#include <cmath>
#include <sstream>

#include "cocos2d.h"

#include "BurstEmitter.h"
#include "DestructionListener.h"
#include "Emitter.h"
#include "EmitterNode.h"
#include "FlowEmitter.h"
#include "IntestineChain.h"
#include "LevelB2D.h"
#include "LevelItemsDrawNode.h"
#include "Ligament.h"
#include "Session.h"
#include "Settings.h"
#include "Sound.h"
#include "SoundController.h"
#include "SpinalCord.h"
#include "StageCamera.h"
#include "Vehicle.h"
#include "platform/compat/Box2DFloat.h"
#include "online/FlashRuntime.h"  // ONLINE (PC addition)
#include "online/vehicles/UserVehicle.h"  // ONLINE (PC addition)
#include "GameplayControls.h"  // QOL (PC addition): re-grab vehicle
#include "qol/QoL.h"           // QOL (PC addition): re-grab vehicle

USING_NS_CC;

// .data @00abb5a8
const char* CharacterB2D::_randomVocals[10] = {
    "Elbow1", "Elbow2", "Knee1", "Knee2", "Shoulder1", "Shoulder2", "Hip1", "Hip2", "Knee1", "Spikes",
};

// ---------------------------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------------------------

// @005895b8
bool CharacterB2D::init(Vec2 origin, std::string name, std::string vocalPrefix,
                        std::string vehicleName, int groupID, bool showGore, bool targetable)
{
    _specialType = SpecialTypeCharacter;
    _groupID = groupID;
    _mainCharacter = false;
    _name = name;
    _vocalPrefix = vocalPrefix;
    _vehicleName = vehicleName;
    _origin = origin;
    _dying = false;
    _dead = false;
    _ejected = false;
    _showGore = showGore;
    _targetable = targetable;
    _vocalPitch = 1.0f;
    _nextVocalPriority = VocalPriorityNone;
    _currentVocalPriority = VocalPriorityNone;

    std::string prefix = getSession()->getMode() != SessionModeGameplay ? "character_select_" : "";
    loadBodies(_name + "_" + _vehicleName);
    loadSpriteFrames("characters/" + prefix + _name + "_sprites.plist");

    createSprites();
    createFilters();
    createBodies();
    createFixtures();
    taperBodies();
    createJoints();
    setLimits();
    addContactListeners();
    createDictionaries();

    _voiceSound = nullptr;
    _helmetBody = nullptr;
    _upperArm1State = 1;
    _upperArm2State = 1;
    _upperLeg1State = 1;
    _upperLeg2State = 1;
    _lowerLeg1State = 1;
    _lowerLeg2State = 1;
    _heartAnchor = Vec2(0.6f, 0.5f);
    _brainAnchor = Vec2(0.5f, 0.6f);
    _shoulderWoundSprite = nullptr;
    _neckWoundSprite = nullptr;
    _upperArm3Body = nullptr;
    _upperArm4Body = nullptr;
    _upperLeg3Body = nullptr;
    _upperLeg4Body = nullptr;
    _intestineChain = nullptr;
    _spinalCord = nullptr;
    _gripJoint1 = nullptr;
    _gripJoint2 = nullptr;
    _neckBloodFlow = nullptr;
    _headBloodFlow = nullptr;
    _shoulder1BloodFlow = nullptr;
    _shoulder2BloodFlow = nullptr;
    _stomachBloodFlow = nullptr;
    _hip1BloodFlow = nullptr;
    _hip2BloodFlow = nullptr;
    _arm1BloodFlow = nullptr;
    _arm2BloodFlow = nullptr;
    _thigh1BloodFlow = nullptr;
    _thigh2BloodFlow = nullptr;

    getLevel()->addToActions(this);
    return true;
}

// @00589acc
void CharacterB2D::createFilters()
{
    _defaultFilter.categoryBits = 0x104;
    _defaultFilter.maskBits = 0x10e;
    _defaultFilter.groupIndex = (int16)_groupID;

    _zeroFilter.categoryBits = 0x104;
    _zeroFilter.maskBits = 0xffff;
    _zeroFilter.groupIndex = 0;

    _lowerBodyFilter.categoryBits = 0x104;
    _lowerBodyFilter.maskBits = 0x10e;
    _lowerBodyFilter.groupIndex = (int16)(_groupID - 5);
}

// @00589b04
// Injury thresholds scale with the body-part masses (the divisors are the Business Guy's masses).
void CharacterB2D::setLimits()
{
    float headRatio = _headBody->GetMass() / 0.054739f;
    float chestRatio = _chestBody->GetMass() / 0.172224f;
    float pelvisRatio = _pelvisBody->GetMass() / 0.073728f;
    float lowerLegRatio = _lowerLeg1Body->GetMass() / 0.1024f;
    float lowerArmRatio = _lowerArm1Body->GetMass() / 0.0768f;
    float upperArmRatio = _upperArm1Body->GetMass() / 0.0704f;
    float upperLegRatio = _upperLeg1Body->GetMass() / 0.1496f;

    _headSmashLimit = headRatio * 3.0f * 1.15f;
    _chestSmashLimit = chestRatio * 7.5f * 1.15f;
    _pelvisSmashLimit = pelvisRatio * 5.5f * 1.15f;
    _footSmashLimit = lowerLegRatio * 4.0f * 1.15f;

    _neckBreakLimit = roundf(headRatio * 85.0f * 2.5f);
    _spineLimit = roundf(headRatio * 105.0f * 2.5f);
    _torsoBreakLimit = roundf(pelvisRatio * 180.0f * 2.5f);
    _intestineLimit = roundf(pelvisRatio * 260.0f * 2.5f);
    _shoulderBreakLimit = roundf(upperArmRatio * 75.0f * 2.5f);
    _shoulderSnapLimit = roundf(upperArmRatio * 90.0f * 2.5f);
    _hipBreakLimit = roundf(upperLegRatio * 95.0f * 2.5f);
    _hipSnapLimit = roundf(upperLegRatio * 110.0f * 2.5f);
    _elbowBreakLimit = roundf(lowerArmRatio * 70.0f * 2.5f);
    _elbowLigamentLimit = roundf(lowerArmRatio * 80.0f * 2.5f);
    _kneeBreakLimit = roundf(lowerLegRatio * 80.0f * 2.5f);
    _kneeLigamentLimit = roundf(lowerLegRatio * 95.0f * 2.5f);

    _jointLimits[_neckJoint] = _neckBreakLimit;
    _jointLimits[_shoulderJoint1] = _shoulderBreakLimit;
    _jointLimits[_shoulderJoint2] = _shoulderBreakLimit;
    _jointLimits[_hipJoint1] = _hipBreakLimit;
    _jointLimits[_hipJoint2] = _hipBreakLimit;
    _jointLimits[_elbowJoint1] = _elbowBreakLimit;
    _jointLimits[_elbowJoint2] = _elbowBreakLimit;
    _jointLimits[_kneeJoint1] = _kneeBreakLimit;
    _jointLimits[_kneeJoint2] = _kneeBreakLimit;
    _jointLimits[_waistJoint] = _torsoBreakLimit;
}

// @0058a27c
void CharacterB2D::addContactListeners()
{
    addToPostSolve(_headFixture);
    addToPostSolve(_chestFixture);
    addToPostSolve(_pelvisFixture);
    addToPostSolve(_lowerLeg1Fixture);
    addToPostSolve(_lowerLeg2Fixture);
}

// @0058a2cc
void CharacterB2D::createDictionaries()
{
    if (_helmetOn) {
        _contactImpulseDict[_headFixture] = 2.0f;
    } else {
        _contactImpulseDict[_headFixture] = _headSmashLimit;
    }
    _contactImpulseDict[_chestFixture] = _chestSmashLimit;
    _contactImpulseDict[_pelvisFixture] = _pelvisSmashLimit;
    _contactImpulseDict[_lowerLeg1Fixture] = _footSmashLimit;
    _contactImpulseDict[_lowerLeg2Fixture] = _footSmashLimit;
    _contactImpulseDict[_lowerArm1Fixture] = 0.0f;
    _contactImpulseDict[_lowerArm2Fixture] = 0.0f;
}

// @0058a7dc (D1), @0058a9e4 (D0); @0058a96c is the inlined ValueMap destructor.
CharacterB2D::~CharacterB2D()
{
    CC_SAFE_RELEASE_NULL(_intestineChain);
    CC_SAFE_RELEASE(_spinalCord);
    for (int i = 0; i < _composites.size(); i++) {
        _composites[i]->release();
    }
}

// ---------------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------------

// @0058aa08
void CharacterB2D::setMainCharacter(bool mainCharacter)
{
    _mainCharacter = mainCharacter;
}

// @0058aa10
int CharacterB2D::getGroupIndex()
{
    return _groupID;
}

// @0058aa18
bool CharacterB2D::getDead()
{
    return _dead;
}

// @0058aa20
bool CharacterB2D::getEjected()
{
    return _ejected;
}

// @0058aa28
b2Body* CharacterB2D::getFocus()
{
    return _cameraFocus;
}

// @0058aa30
void CharacterB2D::doNothing()
{
    if (_ejected) {
        endGrab();
        setCurrentPose(CharacterPoseNone);
    }
}

// @0058aa7c
// Riding: the control bits drive the vehicle. Ejected: they select a pose and grab.
void CharacterB2D::setState(unsigned char state)
{
    // ONLINE (PC addition): riding a browser user vehicle, the controls drive it (Flash
    // checkKeyStates -> userVehicle.operateKeys).
    if (online::flashLevel() && onlineDriveUserVehicle(state)) {
        return;
    }
    if (_ejected) {
        if (state == 0) {
            doNothing();
            return;
        }
        if (state & 0x01) {
            setCurrentPose(CharacterPoseSuperman);
        } else if (state & 0x02) {
            setCurrentPose(CharacterPoseTuck);
        } else if (state & 0x04) {
            setCurrentPose(CharacterPoseArch);
        } else if (state & 0x08) {
            setCurrentPose(CharacterPosePushup);
        } else {
            setCurrentPose(CharacterPoseNone);
        }
        if (state & 0x10) {
            startGrab();
        } else {
            endGrab();
        }
        return;
    }

    if (_vehicle) {
        if (state & 0x01) {
            _vehicle->forwardButtonPressed();
        } else if (state & 0x02) {
            _vehicle->backButtonPressed();
        } else {
            _vehicle->forwardBackButtonsNull();
        }
        if (state & 0x04) {
            _vehicle->leanForwardButtonPressed();
        } else if (state & 0x08) {
            _vehicle->leanBackButtonPressed();
        } else {
            _vehicle->leanButtonsNull();
        }
        if (state & 0x10) {
            _vehicle->special1ButtonPressed();
        } else {
            _vehicle->special1ButtonNull();
        }
        if (state & 0x80) {
            ejectBtnPressed();
        }
    }
}

// @0058abf4
void CharacterB2D::ejectBtnPressed()
{
    if (_vehicle) {
        _vehicle->ejectAllCharacters();
    }
}

// @0058ac0c
void CharacterB2D::loadBodies(std::string name)
{
    std::string path = "characters/bodies/" + name + ".plist";
    std::string fullPath = FileUtils::getInstance()->fullPathForFilename(path);
    _bodiesDict = FileUtils::getInstance()->getValueMapFromFile(fullPath);
}

// @0058ad9c
// Remembers, per watched fixture, the strongest contact of the step that exceeds its threshold.
void CharacterB2D::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                             const b2ContactImpulse* impulse)
{
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        maxImpulse = b2Max(maxImpulse, impulse->normalImpulses[1]);
    }
    if (_contactImpulseDict[fixture] < maxImpulse) {
        LevelItemContact& result = _contactResultBufferDict[fixture];
        result.fixture = fixture;
        result.otherFixture = otherFixture;
        result.impulse = maxImpulse;
    }
}

// ---------------------------------------------------------------------------------------------
// Rig creation
// ---------------------------------------------------------------------------------------------

// @0058af3c
// Sprite stacking: arm 2 / leg 2 behind (character background), head/chest/pelvis in the
// midground, leg 1 / arm 1 in front (character foreground).
void CharacterB2D::createSprites()
{
    Node* background = getSession()->getCharacterBackground();
    _lowerArm2Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerArm2_1.png");
    background->addChild(_lowerArm2Sprite, 0);
    _lowerArmOpen2Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerArm2_1_open.png");
    _lowerArmOpen2Sprite->setVisible(false);
    background->addChild(_lowerArmOpen2Sprite, 1);
    _upperArm2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm2_1.png");
    background->addChild(_upperArm2Sprite, 2);
    _upperLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg2_1.png");
    background->addChild(_upperLeg2Sprite, 3);
    _lowerLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerLeg2_1.png");
    background->addChild(_lowerLeg2Sprite, 4);

    Node* midground = getSession()->getCharacterMidground();
    _headSprite = Sprite::createWithSpriteFrameName(_name + "_head_1.png");
    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    if (bodies.find("helmetShape") != bodies.end()) {
        _helmetOn = true;
        Sprite* helmet = createHelmetSprite();
        helmet->setAnchorPoint(Vec2(0, 0));
        _headSprite->addChild(helmet);
    }
    midground->addChild(_headSprite, 5);
    _chestSprite = Sprite::createWithSpriteFrameName(_name + "_chest_1.png");
    midground->addChild(_chestSprite, 6);

    bool shirtAbovePants = false;
    if (_bodiesDict.find("shirtAbovePants") != _bodiesDict.end()) {
        shirtAbovePants = _bodiesDict.at("shirtAbovePants").asBool();
    }
    _pelvisSprite = Sprite::createWithSpriteFrameName(_name + "_pelvis.png");
    int pelvisZOrder;
    int foregroundZOrder;
    if (shirtAbovePants) {
        pelvisZOrder = _chestSprite->getLocalZOrder() - 1;
        foregroundZOrder = 7;
    } else {
        pelvisZOrder = 7;
        foregroundZOrder = 8;
    }
    midground->addChild(_pelvisSprite, pelvisZOrder);

    Node* foreground = getSession()->getCharacterForeground();
    _upperLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg1_1.png");
    foreground->addChild(_upperLeg1Sprite, foregroundZOrder);
    _lowerLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerLeg1_1.png");
    foreground->addChild(_lowerLeg1Sprite, foregroundZOrder + 1);
    _upperArm1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm1_1.png");
    foreground->addChild(_upperArm1Sprite, foregroundZOrder + 2);
    _lowerArm1Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerArm1_1.png");
    foreground->addChild(_lowerArm1Sprite, foregroundZOrder + 3);
    _lowerArmOpen1Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerArm1_1_open.png");
    _lowerArmOpen1Sprite->setVisible(false);
    foreground->addChild(_lowerArmOpen1Sprite, foregroundZOrder + 4);
}

// @0058bfcc
Sprite* CharacterB2D::createHelmetSprite()
{
    Sprite* helmet = Sprite::createWithSpriteFrameName(_name + "_helmet.png");
    helmet->setTag(0);
    return helmet;
}

// @0058c144
void CharacterB2D::createBodies()
{
    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    _headBody = createBody(&bodies.at("headShape").asValueMap(), _origin);
    _chestBody = createBody(&bodies.at("chestShape").asValueMap(), _origin);
    _upperArm1Body = createBody(&bodies.at("upperArm1Shape").asValueMap(), _origin);
    _upperArm2Body = createBody(&bodies.at("upperArm2Shape").asValueMap(), _origin);
    _lowerArm1Body = createBody(&bodies.at("lowerArm1Shape").asValueMap(), _origin);
    _lowerArm2Body = createBody(&bodies.at("lowerArm2Shape").asValueMap(), _origin);
    _pelvisBody = createBody(&bodies.at("pelvisShape").asValueMap(), _origin);
    _upperLeg1Body = createBody(&bodies.at("upperLeg1Shape").asValueMap(), _origin);
    _upperLeg2Body = createBody(&bodies.at("upperLeg2Shape").asValueMap(), _origin);
    _lowerLeg1Body = createBody(&bodies.at("lowerLeg1Shape").asValueMap(), _origin);
    _lowerLeg2Body = createBody(&bodies.at("lowerLeg2Shape").asValueMap(), _origin);

    _headBody->SetUserData(_headSprite);
    _chestBody->SetUserData(_chestSprite);
    _upperArm1Body->SetUserData(_upperArm1Sprite);
    _upperArm2Body->SetUserData(_upperArm2Sprite);
    _lowerArm1Body->SetUserData(_lowerArm1Sprite);
    _lowerArm2Body->SetUserData(_lowerArm2Sprite);
    _pelvisBody->SetUserData(_pelvisSprite);
    _upperLeg1Body->SetUserData(_upperLeg1Sprite);
    _upperLeg2Body->SetUserData(_upperLeg2Sprite);
    _lowerLeg1Body->SetUserData(_lowerLeg1Sprite);
    _lowerLeg2Body->SetUserData(_lowerLeg2Sprite);

    getLevel()->addToPaintBody(_headBody);
    getLevel()->addToPaintBody(_chestBody);
    getLevel()->addToPaintBody(_upperArm1Body);
    getLevel()->addToPaintBody(_upperArm2Body);
    getLevel()->addToPaintBody(_lowerArm1Body);
    getLevel()->addToPaintBody(_lowerArm2Body);
    getLevel()->addToPaintBody(_pelvisBody);
    getLevel()->addToPaintBody(_upperLeg1Body);
    getLevel()->addToPaintBody(_upperLeg2Body);
    getLevel()->addToPaintBody(_lowerLeg1Body);
    getLevel()->addToPaintBody(_lowerLeg2Body);

    setFocus(_chestBody);
}

// @0058c978
void CharacterB2D::setFocus(b2Body* body)
{
    _cameraFocus = body;
    if (_mainCharacter) {
        StageCamera* camera = getSession()->getCamera();
        if (camera) {
            camera->setFocus(_cameraFocus);
        }
    }
}

// @0058c9bc
void CharacterB2D::createFixtures()
{
    createFixtures(false);
}

// @0058c9cc
void CharacterB2D::createFixtures(bool unused)
{
    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    _chestFixture = createFixture(_chestBody, _defaultFilter, &bodies.at("chestShape").asValueMap());
    _headFixture = createFixture(_headBody, _defaultFilter, &bodies.at("headShape").asValueMap());
    _upperArm1Fixture =
        createFixture(_upperArm1Body, _defaultFilter, &bodies.at("upperArm1Shape").asValueMap());
    _upperArm2Fixture =
        createFixture(_upperArm2Body, _defaultFilter, &bodies.at("upperArm2Shape").asValueMap());
    _lowerArm1Fixture =
        createFixture(_lowerArm1Body, _defaultFilter, &bodies.at("lowerArm1Shape").asValueMap());
    _lowerArm2Fixture =
        createFixture(_lowerArm2Body, _defaultFilter, &bodies.at("lowerArm2Shape").asValueMap());
    _pelvisFixture = createFixture(_pelvisBody, _defaultFilter, &bodies.at("pelvisShape").asValueMap());
    _upperLeg1Fixture =
        createFixture(_upperLeg1Body, _defaultFilter, &bodies.at("upperLeg1Shape").asValueMap());
    _upperLeg2Fixture =
        createFixture(_upperLeg2Body, _defaultFilter, &bodies.at("upperLeg2Shape").asValueMap());
    _lowerLeg1Fixture =
        createFixture(_lowerLeg1Body, _defaultFilter, &bodies.at("lowerLeg1Shape").asValueMap());
    _lowerLeg2Fixture =
        createFixture(_lowerLeg2Body, _defaultFilter, &bodies.at("lowerLeg2Shape").asValueMap());

    LevelB2D* level = getLevel();
    if (_targetable) {
        level->addFixtureMaterial(_headFixture, 2);
        level->addFixtureMaterial(_chestFixture, 2);
        level->addFixtureMaterial(_pelvisFixture, 2);
    }
    _headFixture->SetUserData(this);
    _chestFixture->SetUserData(this);
    _pelvisFixture->SetUserData(this);

    _headBody->ResetMassData();
    _chestBody->ResetMassData();
    _upperArm1Body->ResetMassData();
    _upperArm2Body->ResetMassData();
    _lowerArm1Body->ResetMassData();
    _lowerArm2Body->ResetMassData();
    _pelvisBody->ResetMassData();
    _upperLeg1Body->ResetMassData();
    _upperLeg2Body->ResetMassData();
    _lowerLeg1Body->ResetMassData();
    _lowerLeg2Body->ResetMassData();

    b2Fixture* fixtures[] = {_chestFixture,     _headFixture,      _upperArm1Fixture, _upperArm2Fixture,
                             _lowerArm1Fixture, _lowerArm2Fixture, _pelvisFixture,    _upperLeg1Fixture,
                             _upperLeg2Fixture, _lowerLeg1Fixture, _lowerLeg2Fixture};
    for (b2Fixture* fixture : fixtures) {
        LevelItemContact& contact = _contactResultBufferDict[fixture];
        contact.impulse = 0.0f;
        contact.fixture = nullptr;
        contact.otherFixture = nullptr;
    }
}

// @0058d9ac
// Revolute joints (bodyA, bodyB, upper, lower limit in degrees); anchors from the plist "joints".
void CharacterB2D::createJoints()
{
    ValueMap joints = _bodiesDict.at("joints").asValueMap();
    _neckJoint = createJoint(_chestBody, _headBody, 20.0f, -20.0f,
                             PointFromString(joints.at("headAnchor").asString()), _origin);
    _shoulderJoint1 = createJoint(_chestBody, _upperArm1Body, 180.0f, -60.0f,
                                  PointFromString(joints.at("upperArmAnchor").asString()), _origin);
    _shoulderJoint2 = createJoint(_chestBody, _upperArm2Body, 180.0f, -60.0f,
                                  PointFromString(joints.at("upperArmAnchor").asString()), _origin);
    _elbowJoint1 = createJoint(_upperArm1Body, _lowerArm1Body, 160.0f, 0.0f,
                               PointFromString(joints.at("lowerArm1Anchor").asString()), _origin);
    _elbowJoint2 = createJoint(_upperArm2Body, _lowerArm2Body, 160.0f, 0.0f,
                               PointFromString(joints.at("lowerArm2Anchor").asString()), _origin);
    _waistJoint = createJoint(_chestBody, _pelvisBody, 5.0f, -5.0f,
                              PointFromString(joints.at("pelvisAnchor").asString()), _origin);
    _hipJoint1 = createJoint(_pelvisBody, _upperLeg1Body, 150.0f, -10.0f,
                             PointFromString(joints.at("upperLegAnchor").asString()), _origin);
    _hipJoint2 = createJoint(_pelvisBody, _upperLeg2Body, 150.0f, -10.0f,
                             PointFromString(joints.at("upperLegAnchor").asString()), _origin);
    _kneeJoint1 = createJoint(_upperLeg1Body, _lowerLeg1Body, 0.0f, -150.0f,
                              PointFromString(joints.at("lowerLeg1Anchor").asString()), _origin);
    _kneeJoint2 = createJoint(_upperLeg2Body, _lowerLeg2Body, 0.0f, -150.0f,
                              PointFromString(joints.at("lowerLeg2Anchor").asString()), _origin);

    _shoulderBloodFlowPos = _shoulderJoint1->GetLocalAnchorA();
    _pelvisBloodFlowPos = _hipJoint1->GetLocalAnchorA();

    _jointsToCheck.push_back(_neckJoint);
    _jointsToCheck.push_back(_shoulderJoint1);
    _jointsToCheck.push_back(_shoulderJoint2);
    _jointsToCheck.push_back(_hipJoint1);
    _jointsToCheck.push_back(_hipJoint2);
    _jointsToCheck.push_back(_elbowJoint1);
    _jointsToCheck.push_back(_elbowJoint2);
    _jointsToCheck.push_back(_kneeJoint1);
    _jointsToCheck.push_back(_kneeJoint2);
    _jointsToCheck.push_back(_waistJoint);
}

// ---------------------------------------------------------------------------------------------
// Per-frame processing
// ---------------------------------------------------------------------------------------------

// @0058eec8
void CharacterB2D::removeFromJointsToCheck(b2Joint* joint)
{
    std::vector<b2RevoluteJoint*>::iterator it =
        std::find(_jointsToCheck.begin(), _jointsToCheck.end(), joint);
    if (it != _jointsToCheck.end()) {
        _jointsToCheck.erase(it);
    }
}

// @0058ef2c
void CharacterB2D::removeFromContactResultBufferDict(b2Fixture* fixture)
{
    _contactResultBufferDict.erase(fixture);
}

// @0058eff0
void CharacterB2D::actions()
{
    checkPose();
    if (!_dead) {
        checkVocals();
        if (_dying) {
            checkBleedOut();
        }
    }
    handleContactResults();
    // QOL (PC addition): re-grab vehicle - time off the vehicle, and a hand on it.
    if (_ejected) {
        _qolEjectedTime += LevelItem::s_timeStep;
        qolCheckRemount();
    }
    if (_showGore) {
        checkJoints();
    }
    if (_intestineChain) {
        _intestineChain->paint();
    }
    if (_spinalCord) {
        _spinalCord->paint();
    }
}

// @0058f100
void CharacterB2D::checkPose()
{
    if (!_dead) {
        switch (_currentPose) {
        case CharacterPoseSuperman:
            supermanPose();
            break;
        case CharacterPoseTuck:
            tuckPose();
            break;
        case CharacterPoseArch:
            archPose();
            break;
        case CharacterPosePushup:
            pushupPose();
            break;
        default:
            // ONLINE (PC addition): user-vehicle rider poses 10..12 (Flash checkPose).
            if (online::flashLevel()) {
                onlineUserVehiclePose();
            }
            break;
        }
    }
}

// @0058f144
// Plays the queued voice clip unless the clip already playing has an equal or higher priority.
void CharacterB2D::checkVocals()
{
    if (_nextVocalPriority != VocalPriorityNone) {
        if (_voiceSound) {
            if (_nextVocalPriority <= _currentVocalPriority) {
                _nextVocalPriority = VocalPriorityNone;
                return;
            }
            _voiceSound->soundFinishedPlaying();
        }
        std::string soundName = _vocalPrefix + _nextVocalString;
        _voiceSound = createBodySound(soundName, _headBody, _vocalPitch, false);
        if (_voiceSound) {
            _currentVocalPriority = _nextVocalPriority;
            // CharacterB2D::checkVocals()::$_0 (@005a1f6c..@005a1fe8: its std::function plumbing)
            _voiceSound->setFinishCallback([this](int&) { voiceSoundFinishedPlaying(); });
        } else {
            _currentVocalPriority = VocalPriorityNone;
        }
        _nextVocalPriority = VocalPriorityNone;
    }
}

// @0058f3d0
void CharacterB2D::checkBleedOut()
{
    _bleedTimeCounter += LevelItem::s_timeStep;
    if (_bleedTimeCounter >= 5.0f) {
        setDead(true);
    }
}

// @0058f400
// Applies the impacts postSolve recorded above each fixture threshold.
void CharacterB2D::handleContactResults()
{
    if (_contactResultBufferDict[_headFixture].impulse > 0.0f) {
        if (!_helmetOn || _helmetBody) {
            headSmash(_contactResultBufferDict[_headFixture].impulse);
        } else {
            helmetSmash(_contactResultBufferDict[_headFixture].impulse);
        }
    }
    if (_contactResultBufferDict[_chestFixture].impulse > 0.0f) {
        chestSmash(_contactResultBufferDict[_chestFixture].impulse);
    }
    if (_contactResultBufferDict[_pelvisFixture].impulse > 0.0f) {
        pelvisSmash(_contactResultBufferDict[_pelvisFixture].impulse);
    }
    if (_contactResultBufferDict[_lowerLeg1Fixture].impulse > 0.0f) {
        foot1Smash(_contactResultBufferDict[_lowerLeg1Fixture].impulse);
    }
    if (_contactResultBufferDict[_lowerLeg2Fixture].impulse > 0.0f) {
        foot2Smash(_contactResultBufferDict[_lowerLeg2Fixture].impulse);
    }
    if (_contactResultBufferDict[_lowerArm1Fixture].impulse > 0.0f) {
        _contactResultBufferDict[_lowerArm1Fixture].impulse = 0.0f;
        // ONLINE (PC addition): a browser user-vehicle handle attaches the rider (Flash grabAction).
        // QOL (PC addition): re-grab vehicle - his own vehicle puts him back on it.
        if ((!online::flashLevel() ||
             !onlineGrabUserVehicle(1, _contactResultBufferDict[_lowerArm1Fixture].otherFixture)) &&
            !qolGrabRemount(1, _contactResultBufferDict[_lowerArm1Fixture].otherFixture->GetBody()))
        grabAction1(_contactResultBufferDict[_lowerArm1Fixture].otherFixture->GetBody());
    }
    if (_contactResultBufferDict[_lowerArm2Fixture].impulse > 0.0f) {
        _contactResultBufferDict[_lowerArm2Fixture].impulse = 0.0f;
        // ONLINE (PC addition): see above. QOL (PC addition): see above.
        if ((!online::flashLevel() ||
             !onlineGrabUserVehicle(2, _contactResultBufferDict[_lowerArm2Fixture].otherFixture)) &&
            !qolGrabRemount(2, _contactResultBufferDict[_lowerArm2Fixture].otherFixture->GetBody()))
        grabAction2(_contactResultBufferDict[_lowerArm2Fixture].otherFixture->GetBody());
    }
}

// @0058ff94
// A joint breaks when its reaction force exceeds its limit, or when its anchors were pulled apart.
// breakJoint() removes the joint from _jointsToCheck, so the index only advances otherwise (the
// original walks the vector with a pointer that stays on the same slot).
void CharacterB2D::checkJoints()
{
    size_t index = 0;
    while (index < _jointsToCheck.size()) {
        b2Joint* joint = _jointsToCheck[index];
        float force = joint->GetReactionForce(getTimeStepInverse()).Length();
        if (force > _jointLimits[joint]) {
            breakJoint(joint, force);
        } else if ((joint->GetAnchorB() - joint->GetAnchorA()).LengthSquared() > 0.5f) {
            breakJoint(joint, 1000.0f);
        } else {
            index++;
        }
    }
    if (_spinalCord) {
        _spinalCord->checkJoints();
    }
    if (_intestineChain) {
        _intestineChain->checkJoints();
    }
    for (int i = 0; i < _composites.size(); i++) {
        _composites[i]->checkJoints();
    }
}

// @00590178
void CharacterB2D::breakJoint(const b2Joint* joint, float force)
{
    if (joint == _neckJoint) {
        neckBreak(force, true, true);
    } else if (joint == _waistJoint) {
        torsoBreak(force, true, true, false);
    } else if (joint == _shoulderJoint1) {
        shoulderBreak1(force, true);
    } else if (joint == _shoulderJoint2) {
        shoulderBreak2(force, true);
    } else if (joint == _elbowJoint1) {
        elbowBreak1(force);
    } else if (joint == _elbowJoint2) {
        elbowBreak2(force);
    } else if (joint == _hipJoint1) {
        hipBreak1(force, true);
    } else if (joint == _hipJoint2) {
        hipBreak2(force, true);
    } else if (joint == _kneeJoint1) {
        kneeBreak1(force);
    } else if (joint == _kneeJoint2) {
        kneeBreak2(force);
    }
}

// ---------------------------------------------------------------------------------------------
// Injuries
// ---------------------------------------------------------------------------------------------

// @00590240
// Decapitation. Below the spine limit the head stays attached by a spinal cord whose number of
// vertebrae grows with the force.
void CharacterB2D::neckBreak(float force, bool blood, bool sound)
{
    removeFromJointsToCheck(_neckJoint);
    getWorld()->DestroyJoint(_neckJoint);
    _neckJoint = nullptr;
    addNeckWoundToHead();
    addNeckWoundToChest();
    setDead(true);
    _headBody->GetFixtureList()->SetFilterData(_zeroFilter);
    addHeadBloodFlow();
    if (blood) {
        addNeckBloodFlow();
    }
    if (force < _spineLimit) {
        float breakLimit = _neckBreakLimit;
        ValueMap ligamentShape =
            _bodiesDict.at("bodies").asValueMap().at("ligamentShape").asValueMap();
        Size size = SizeFromString(ligamentShape.at("size").asString());
        Vec2 anchor =
            PointFromString(_bodiesDict.at("joints").asValueMap().at("spineAnchor").asString());
        unsigned int totalVertebrae =
            ceilf(((force - breakLimit) / (_spineLimit - breakLimit)) * 9.0f) + 1.0f;
        _spinalCord = SpinalCord::create(_name, getPtm(), getSession()->getTimeStep(), _chestBody,
                                         b2Vec2(anchor.x, anchor.y), _headBody, totalVertebrae,
                                         size.width);
        _spinalCord->retain();
        getSession()->getBackgroundDrawNode()->addSpinalCord(_spinalCord);
    }
    if (sound) {
        createBodySound("NeckBreak", _headBody, 1.0f, false);
    }
    postInjury(CharacterInjuryNeckBreak);
}

// @005907e4
// Torn in half at the waist: the chest gets its torn sprite, the lower body its own collision
// group. Below the intestine limit an intestine chain links the halves.
void CharacterB2D::torsoBreak(float force, bool blood, bool sound, bool unusedFlag)
{
    removeFromJointsToCheck(_waistJoint);
    if (!_dead) {
        setDying(true);
    }
    getWorld()->DestroyJoint(_waistJoint);
    _waistJoint = nullptr;

    int zOrder = _chestSprite->getLocalZOrder();
    Node* parent = _chestSprite->getParent();
    Sprite* shoulderWound = _shoulderWoundSprite;
    Sprite* neckWound = _neckWoundSprite;
    _chestSprite->removeFromParentAndCleanup(false);
    _chestSprite = nullptr;
    _chestSprite = Sprite::createWithSpriteFrameName(_name + "_chest_2.png");
    parent->addChild(_chestSprite, zOrder);
    _chestBody->SetUserData(_chestSprite);
    if (shoulderWound) {
        addShoulderWoundToChest();
    }
    if (neckWound) {
        addNeckWoundToChest();
    }
    addPelvisWoundToPelvis();

    _pelvisBody->GetFixtureList()->SetFilterData(_lowerBodyFilter);
    if (_upperLeg1Body->GetFixtureList()->GetFilterData().groupIndex == _defaultFilter.groupIndex) {
        _upperLeg1Body->GetFixtureList()->SetFilterData(_lowerBodyFilter);
    }
    if (_upperLeg2Body->GetFixtureList()->GetFilterData().groupIndex == _defaultFilter.groupIndex) {
        _upperLeg2Body->GetFixtureList()->SetFilterData(_lowerBodyFilter);
    }
    if (_lowerLeg1Body->GetFixtureList()->GetFilterData().groupIndex == _defaultFilter.groupIndex) {
        _lowerLeg1Body->GetFixtureList()->SetFilterData(_lowerBodyFilter);
    }
    if (_lowerLeg2Body->GetFixtureList()->GetFilterData().groupIndex == _defaultFilter.groupIndex) {
        _lowerLeg2Body->GetFixtureList()->SetFilterData(_lowerBodyFilter);
    }

    if (force < _intestineLimit) {
        float breakLimit = _torsoBreakLimit;
        ValueMap intestineShape =
            _bodiesDict.at("bodies").asValueMap().at("intestineShape").asValueMap();
        Size size = SizeFromString(intestineShape.at("size").asString());
        // NOTE(sic, @00590c44): the force is not part of the formula (unlike neckBreak), so the
        // count only depends on the two limits. Kept as in the binary.
        unsigned int totalIntestines =
            ceilf((breakLimit / (_intestineLimit - breakLimit)) * 8.0f) + 1.0f;
        _intestineChain = IntestineChain::create(_name, getPtm(), getSession()->getTimeStep(),
                                                 _chestBody, _pelvisBody, totalIntestines,
                                                 size.width);
        _intestineChain->retain();
    }
    if (blood) {
        addStomchBloodFlow();
    }
    if (sound) {
        createBodySound("LimbRip1", _pelvisBody, 1.0f, false);
    }
    addVocalsWithName("Torso", VocalPriority6);
    postInjury(CharacterInjuryTorsoBreak);
}

// @00590f18
// Up to the snap limit the shoulder only dislocates: the upper arm is split and its lower half
// (_upperArm3Body, "sevArm1Shape") is re-attached to the chest. Beyond it the arm is torn off.
void CharacterB2D::shoulderBreak1(float force, bool blood)
{
    removeFromJointsToCheck(_shoulderJoint1);
    Node* parent = _upperArm1Sprite->getParent();
    if (force <= _shoulderSnapLimit) {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        b2FixtureDef fixtureDef;
        b2PolygonShape shape;
        b2Vec2 anchor = _shoulderJoint1->GetAnchorA();
        bodyDef.position = anchor;
        bodyDef.angle = _upperArm1Body->GetAngle();
        fixtureDef.filter = _defaultFilter;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = 0.3f;
        fixtureDef.restitution = 0.1f;
        ValueMap armShape = _bodiesDict.at("bodies").asValueMap().at("sevArm1Shape").asValueMap();
        Size size = SizeFromString(armShape.at("size").asString());
        Vec2 position = PointFromString(armShape.at("pos").asString());
        shape.SetAsBox(size.width, size.height, b2Vec2(0.0f, position.y), 0.0f);
        fixtureDef.shape = &shape;
        _upperArm3Body = getWorld()->CreateBody(&bodyDef);
        _upperArm3Body->CreateFixture(&fixtureDef);
        _upperArm3Body->ResetMassData();
        _upperArm3Body->SetLinearVelocity(_upperArm1Body->GetLinearVelocity());
        _upperArm3Body->SetAngularVelocity(_upperArm1Body->GetAngularVelocity());

        getWorld()->DestroyJoint(_shoulderJoint1);
        b2RevoluteJointDef jointDef;
        float referenceAngle = _upperArm3Body->GetAngle() - _chestBody->GetAngle();
        jointDef.enableLimit = true;
        jointDef.lowerAngle = -1.0471976f - referenceAngle;
        jointDef.upperAngle = 2.7925267f - referenceAngle;
        jointDef.Initialize(_chestBody, _upperArm3Body, anchor);
        _shoulderJoint1 = (b2RevoluteJoint*)getWorld()->CreateJoint(&jointDef);

        b2Fixture* fixture = _upperArm1Body->GetFixtureList();
        fixture->SetFilterData(_zeroFilter);
        getLevel()->addFixtureMaterial(fixture, 1);
        fixture->SetUserData(this);
        _lowerArm1Body->GetFixtureList()->SetFilterData(_zeroFilter);

        int zOrder = _upperArm1Sprite->getLocalZOrder();
        _upperArm1Sprite->removeFromParentAndCleanup(false);
        if (_upperArm1State == 5) {
            _upperArm1State = 6;
            _upperArm1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm1_6.png");
        } else {
            _upperArm1State = 3;
            _upperArm1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm1_3.png");
        }
        _upperArm3Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm1_2.png");
        parent->addChild(_upperArm1Sprite);
        parent->addChild(_upperArm3Sprite, zOrder);
        _upperArm1Body->SetUserData(_upperArm1Sprite);
        _upperArm3Body->SetUserData(_upperArm3Sprite);
        getLevel()->addToPaintBody(_upperArm3Body);
        if (blood) {
            addShoulder1BloodFlow(force);
        }
        createBodySound("LimbRip2", _upperArm1Body, 1.0f, false);
        addVocalsWithName("Shoulder1", VocalPriority4);
    } else {
        getWorld()->DestroyJoint(_shoulderJoint1);
        _shoulderJoint1 = nullptr;
        b2Fixture* fixture = _upperArm1Body->GetFixtureList();
        fixture->SetFilterData(_zeroFilter);
        getLevel()->addFixtureMaterial(fixture, 1);
        fixture->SetUserData(this);
        _lowerArm1Body->GetFixtureList()->SetFilterData(_zeroFilter);

        int zOrder = _chestSprite->getLocalZOrder();
        _upperArm1Sprite->removeFromParentAndCleanup(false);
        if (_upperArm1State == 5) {
            _upperArm1State = 7;
            _upperArm1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm1_7.png");
        } else {
            _upperArm1State = 4;
            _upperArm1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm1_4.png");
        }
        parent->addChild(_upperArm1Sprite, zOrder);
        _upperArm1Body->SetUserData(_upperArm1Sprite);
        addShoulderWoundToChest();
        if (blood) {
            addShoulder1BloodFlow(force);
            createBodySound("LimbRip2", _upperArm1Body, 1.0f, false);
        }
        addArm1BloodFlow();
        addVocalsWithName("Shoulder1", VocalPriority4);
    }
    if (_elbowJoint1) {
        float angle = (_lowerArm1Body->GetAngle() - _upperArm1Body->GetAngle()) -
                      owb2::jointAngle(_elbowJoint1);
        _elbowJoint1->SetLimits(0.0f - angle, 2.7925267f - angle);
    }
    postInjury(CharacterInjuryShoulder1Break);
}

// @00591cf0
// Mirror of shoulderBreak1 with small differences of the original: the dislocated joint gets a
// max motor torque of 4, the dislocation sound only plays with blood, the torn-off arm keeps
// its own z-order and leaves no shoulder wound, and the joint pointer is cleared last.
void CharacterB2D::shoulderBreak2(float force, bool blood)
{
    removeFromJointsToCheck(_shoulderJoint2);
    Node* parent = _upperArm2Sprite->getParent();
    if (force <= _shoulderSnapLimit) {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        b2FixtureDef fixtureDef;
        b2PolygonShape shape;
        b2Vec2 anchor = _shoulderJoint2->GetAnchorA();
        bodyDef.position = anchor;
        bodyDef.angle = _upperArm2Body->GetAngle();
        fixtureDef.filter = _defaultFilter;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = 0.3f;
        fixtureDef.restitution = 0.1f;
        ValueMap armShape = _bodiesDict.at("bodies").asValueMap().at("sevArm1Shape").asValueMap();
        Size size = SizeFromString(armShape.at("size").asString());
        Vec2 position = PointFromString(armShape.at("pos").asString());
        shape.SetAsBox(size.width, size.height, b2Vec2(0.0f, position.y), 0.0f);
        fixtureDef.shape = &shape;
        _upperArm4Body = getWorld()->CreateBody(&bodyDef);
        _upperArm4Body->CreateFixture(&fixtureDef);
        _upperArm4Body->ResetMassData();
        _upperArm4Body->SetLinearVelocity(_upperArm2Body->GetLinearVelocity());
        _upperArm4Body->SetAngularVelocity(_upperArm2Body->GetAngularVelocity());

        getWorld()->DestroyJoint(_shoulderJoint2);
        b2RevoluteJointDef jointDef;
        float referenceAngle = _upperArm4Body->GetAngle() - _chestBody->GetAngle();
        jointDef.enableLimit = true;
        jointDef.lowerAngle = -1.0471976f - referenceAngle;
        jointDef.upperAngle = 2.7925267f - referenceAngle;
        jointDef.maxMotorTorque = 4.0f;
        jointDef.Initialize(_chestBody, _upperArm4Body, anchor);
        _shoulderJoint2 = (b2RevoluteJoint*)getWorld()->CreateJoint(&jointDef);

        b2Fixture* fixture = _upperArm2Body->GetFixtureList();
        fixture->SetFilterData(_zeroFilter);
        getLevel()->addFixtureMaterial(fixture, 1);
        fixture->SetUserData(this);
        _lowerArm2Body->GetFixtureList()->SetFilterData(_zeroFilter);

        int zOrder = _upperArm2Sprite->getLocalZOrder();
        _upperArm2Sprite->removeFromParentAndCleanup(false);
        if (_upperArm2State == 5) {
            _upperArm2State = 6;
            _upperArm2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm2_6.png");
        } else {
            _upperArm2State = 3;
            _upperArm2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm2_3.png");
        }
        _upperArm4Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm2_2.png");
        parent->addChild(_upperArm2Sprite);
        parent->addChild(_upperArm4Sprite, zOrder);
        _upperArm2Body->SetUserData(_upperArm2Sprite);
        _upperArm4Body->SetUserData(_upperArm4Sprite);
        getLevel()->addToPaintBody(_upperArm4Body);
        if (blood) {
            addShoulder2BloodFlow(force);
            createBodySound("LimbRip2", _upperArm2Body, 1.0f, false);
        }
        addVocalsWithName("Shoulder2", VocalPriority4);
    } else {
        getWorld()->DestroyJoint(_shoulderJoint2);
        b2Fixture* fixture = _upperArm2Body->GetFixtureList();
        fixture->SetFilterData(_zeroFilter);
        getLevel()->addFixtureMaterial(fixture, 1);
        fixture->SetUserData(this);
        _lowerArm2Body->GetFixtureList()->SetFilterData(_zeroFilter);

        int zOrder = _upperArm2Sprite->getLocalZOrder();
        _upperArm2Sprite->removeFromParentAndCleanup(false);
        if (_upperArm2State == 5) {
            _upperArm2State = 7;
            _upperArm2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm2_7.png");
        } else {
            _upperArm2State = 4;
            _upperArm2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm2_4.png");
        }
        parent->addChild(_upperArm2Sprite, zOrder);
        _upperArm2Body->SetUserData(_upperArm2Sprite);
        if (blood) {
            addShoulder2BloodFlow(force);
            createBodySound("LimbRip2", _upperArm2Body, 1.0f, false);
        }
        addArm2BloodFlow();
        addVocalsWithName("Shoulder2", VocalPriority4);
        _shoulderJoint2 = nullptr;
    }
    if (_elbowJoint2) {
        float angle = (_lowerArm2Body->GetAngle() - _upperArm2Body->GetAngle()) -
                      owb2::jointAngle(_elbowJoint2);
        _elbowJoint2->SetLimits(0.0f - angle, 2.7925267f - angle);
    }
    postInjury(CharacterInjuryShoulder2Break);
}

// @00592acc
// Forearm torn off at the elbow; below the ligament limit a ligament keeps it attached.
void CharacterB2D::elbowBreak1(float force)
{
    removeFromJointsToCheck(_elbowJoint1);
    _lowerArmOpen1Sprite->setVisible(false);
    getWorld()->DestroyJoint(_elbowJoint1);
    _elbowJoint1 = nullptr;
    _lowerArm1Body->GetFixtureList()->SetFilterData(_zeroFilter);

    Node* parent = _upperArm1Sprite->getParent();
    int zOrder = _upperArm1Sprite->getLocalZOrder();
    _upperArm1Sprite->removeFromParentAndCleanup(false);
    if (_upperArm1State == 4) {
        _upperArm1State = 7;
        _upperArm1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm1_7.png");
    } else if (_upperArm1State == 3) {
        _upperArm1State = 6;
        _upperArm1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm1_6.png");
    } else {
        _upperArm1State = 5;
        _upperArm1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm1_5.png");
    }
    parent->addChild(_upperArm1Sprite, zOrder);
    zOrder = _lowerArm1Sprite->getLocalZOrder();
    _lowerArm1Sprite->removeFromParentAndCleanup(false);
    _lowerArm1Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerArm1_2.png");
    parent->addChild(_lowerArm1Sprite, zOrder);
    _upperArm1Body->SetUserData(_upperArm1Sprite);
    _lowerArm1Body->SetUserData(_lowerArm1Sprite);

    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        b2PolygonShape* shape = (b2PolygonShape*)_lowerArm1Body->GetFixtureList()->GetShape();
        BurstEmitter* burst = BurstEmitter::createBloodBurst(
            5.0f, 15.0f, _lowerArm1Body, b2Vec2(0.0f, shape->m_vertices[2].y), 50);
        if (burst) {
            particles->addChild(burst);
        }
    }
    if (force < _elbowLigamentLimit) {
        ValueMap ligamentShape =
            _bodiesDict.at("bodies").asValueMap().at("ligamentShape").asValueMap();
        Size size = SizeFromString(ligamentShape.at("size").asString());
        Ligament* ligament = Ligament::create(getPtm(), getSession()->getTimeStep(),
                                              _upperArm1Body, _lowerArm1Body, size.width);
        ligament->retain();
        _composites.push_back(ligament);
        getSession()->getMidgroundDrawNode()->addLigament(ligament);
    }
    // rand() / 2^31 (bionic RAND_MAX + 1): "BoneBreak1" .. "BoneBreak4".
    int boneBreakNumber = ceilf((float)rand() * 4.656613e-10f * 4.0f);
    std::stringstream soundName;
    soundName << "BoneBreak" << boneBreakNumber;
    createBodySound(soundName.str(), _lowerArm1Body, 1.0f, false);
    if (_shoulderJoint1 && !_upperArm3Body) {
        addVocalsWithName("Elbow1", VocalPriority2);
    }
    postInjury(CharacterInjuryElbow1Break);
}

// @005935c0
// Mirror of elbowBreak1 (blood burst also in the particle foreground; the ligament is drawn in
// the background draw node).
void CharacterB2D::elbowBreak2(float force)
{
    removeFromJointsToCheck(_elbowJoint2);
    _lowerArmOpen2Sprite->setVisible(false);
    getWorld()->DestroyJoint(_elbowJoint2);
    _elbowJoint2 = nullptr;
    _lowerArm2Body->GetFixtureList()->SetFilterData(_zeroFilter);

    Node* parent = _upperArm2Sprite->getParent();
    int zOrder = _upperArm2Sprite->getLocalZOrder();
    _upperArm2Sprite->removeFromParentAndCleanup(false);
    if (_upperArm2State == 4) {
        _upperArm2State = 7;
        _upperArm2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm2_7.png");
    } else if (_upperArm2State == 3) {
        _upperArm2State = 6;
        _upperArm2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm2_6.png");
    } else {
        _upperArm2State = 5;
        _upperArm2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm2_5.png");
    }
    parent->addChild(_upperArm2Sprite, zOrder);
    zOrder = _lowerArm2Sprite->getLocalZOrder();
    _lowerArm2Sprite->removeFromParentAndCleanup(false);
    _lowerArm2Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerArm2_2.png");
    parent->addChild(_lowerArm2Sprite, zOrder);
    _upperArm2Body->SetUserData(_upperArm2Sprite);
    _lowerArm2Body->SetUserData(_lowerArm2Sprite);

    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        b2PolygonShape* shape = (b2PolygonShape*)_lowerArm2Body->GetFixtureList()->GetShape();
        BurstEmitter* burst = BurstEmitter::createBloodBurst(
            5.0f, 15.0f, _lowerArm2Body, b2Vec2(0.0f, shape->m_vertices[2].y), 50);
        if (burst) {
            particles->addChild(burst);
        }
    }
    if (force < _elbowLigamentLimit) {
        ValueMap ligamentShape =
            _bodiesDict.at("bodies").asValueMap().at("ligamentShape").asValueMap();
        Size size = SizeFromString(ligamentShape.at("size").asString());
        Ligament* ligament = Ligament::create(getPtm(), getSession()->getTimeStep(),
                                              _upperArm2Body, _lowerArm2Body, size.width);
        ligament->retain();
        _composites.push_back(ligament);
        getSession()->getBackgroundDrawNode()->addLigament(ligament);
    }
    int boneBreakNumber = ceilf((float)rand() * 4.656613e-10f * 4.0f);
    std::stringstream soundName;
    soundName << "BoneBreak" << boneBreakNumber;
    createBodySound(soundName.str(), _lowerArm2Body, 1.0f, false);
    if (_shoulderJoint2 && !_upperArm4Body) {
        addVocalsWithName("Elbow2", VocalPriority2);
    }
    postInjury(CharacterInjuryElbow2Break);
}

// @005940b4
// Up to the snap limit the hip only dislocates: the thigh is shortened to "sevLeg2Shape" and
// its other part (_upperLeg3Body, "sevLeg1Shape") is re-attached to the pelvis. Beyond it the
// leg is torn off and the pelvis gets a hip wound.
void CharacterB2D::hipBreak1(float force, bool blood)
{
    removeFromJointsToCheck(_hipJoint1);
    Node* parent = _upperLeg1Sprite->getParent();
    if (force <= _hipSnapLimit) {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        b2FixtureDef fixtureDef;
        b2PolygonShape shape;
        b2Vec2 anchor = _hipJoint1->GetAnchorA();
        bodyDef.position = anchor;
        bodyDef.angle = _upperLeg1Body->GetAngle();
        fixtureDef.filter = _defaultFilter;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = 0.3f;
        fixtureDef.restitution = 0.1f;
        ValueMap legShape = _bodiesDict.at("bodies").asValueMap().at("sevLeg1Shape").asValueMap();
        Size size = SizeFromString(legShape.at("size").asString());
        Vec2 position = PointFromString(legShape.at("pos").asString());
        shape.SetAsBox(size.width, size.height, b2Vec2(0.0f, position.y), 0.0f);
        fixtureDef.shape = &shape;
        _upperLeg3Body = getWorld()->CreateBody(&bodyDef);
        _upperLeg3Body->CreateFixture(&fixtureDef);
        _upperLeg3Body->ResetMassData();
        _upperLeg3Body->SetLinearVelocity(_upperLeg1Body->GetLinearVelocity());
        _upperLeg3Body->SetAngularVelocity(_upperLeg1Body->GetAngularVelocity());

        getWorld()->DestroyJoint(_hipJoint1);
        _hipJoint1 = nullptr;
        b2RevoluteJointDef jointDef;
        float referenceAngle = _upperLeg3Body->GetAngle() - _pelvisBody->GetAngle();
        jointDef.enableLimit = true;
        jointDef.lowerAngle = -0.17453292f - referenceAngle;
        jointDef.upperAngle = 2.6179938f - referenceAngle;
        jointDef.maxMotorTorque = 4.0f;
        jointDef.Initialize(_pelvisBody, _upperLeg3Body, anchor);
        _hipJoint1 = (b2RevoluteJoint*)getWorld()->CreateJoint(&jointDef);

        ValueMap thighShape =
            _bodiesDict.at("bodies").asValueMap().at("sevLeg2Shape").asValueMap();
        Size thighSize = SizeFromString(thighShape.at("size").asString());
        Vec2 thighPosition = PointFromString(thighShape.at("pos").asString());
        b2Fixture* fixture = _upperLeg1Body->GetFixtureList();
        ((b2PolygonShape*)fixture->GetShape())
            ->SetAsBox(thighSize.width, thighSize.height, b2Vec2(0.0f, thighPosition.y), 0.0f);
        _upperLeg1Body->ResetMassData();
        getLevel()->addFixtureMaterial(fixture, 1);
        fixture->SetUserData(this);
        _lowerLeg1Body->GetFixtureList()->SetFilterData(_zeroFilter);

        int zOrder = _upperLeg1Sprite->getLocalZOrder();
        _upperLeg1Sprite->removeFromParentAndCleanup(false);
        if (_upperLeg1State == 5) {
            _upperLeg1State = 6;
            _upperLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg1_6.png");
        } else {
            _upperLeg1State = 3;
            _upperLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg1_3.png");
        }
        _upperLeg3Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg1_2.png");
        parent->addChild(_upperLeg1Sprite);
        parent->addChild(_upperLeg3Sprite, zOrder);
        _upperLeg1Body->SetUserData(_upperLeg1Sprite);
        _upperLeg3Body->SetUserData(_upperLeg3Sprite);
        getLevel()->addToPaintBody(_upperLeg3Body);
        if (blood) {
            addHip1BloodFlow(force);
            createBodySound("LimbRip3", _upperLeg1Body, 1.0f, false);
        }
        addThigh1BloodFlow(force);
        if (_waistJoint) {
            addVocalsWithName("Hip1", VocalPriority5);
        }
    } else {
        getWorld()->DestroyJoint(_hipJoint1);
        _hipJoint1 = nullptr;
        b2Fixture* fixture = _upperLeg1Body->GetFixtureList();
        fixture->SetFilterData(_zeroFilter);
        getLevel()->addFixtureMaterial(fixture, 1);
        fixture->SetUserData(this);
        _lowerLeg1Body->GetFixtureList()->SetFilterData(_zeroFilter);

        int zOrder = _upperLeg1Sprite->getLocalZOrder();
        _upperLeg1Sprite->removeFromParentAndCleanup(false);
        if (_upperLeg1State == 5) {
            _upperLeg1State = 7;
            _upperLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg1_7.png");
        } else {
            _upperLeg1State = 4;
            _upperLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg1_4.png");
        }
        parent->addChild(_upperLeg1Sprite, zOrder);
        _upperLeg1Body->SetUserData(_upperLeg1Sprite);
        Sprite* hipWound = Sprite::createWithSpriteFrameName(_name + "_hipWound.png");
        hipWound->setAnchorPoint(Vec2(0, 0));
        _pelvisSprite->addChild(hipWound);
        if (blood) {
            addHip1BloodFlow(force);
            createBodySound("LimbRip3", _upperLeg1Body, 1.0f, false);
        }
        addThigh1BloodFlow(force);
        if (_waistJoint) {
            addVocalsWithName("Hip1", VocalPriority5);
        }
    }
    if (_kneeJoint1) {
        float angle = (_lowerLeg1Body->GetAngle() - _upperLeg1Body->GetAngle()) -
                      owb2::jointAngle(_kneeJoint1);
        _kneeJoint1->SetLimits(-2.6179938f - angle, 0.0f - angle);
    }
    postInjury(CharacterInjuryHip1Break);
}

// @005951dc
// Mirror of hipBreak1, except that the sprites of leg 2 are re-added without z-order, no hip
// wound is added, and the fixture definition's filter is overwritten after use.
void CharacterB2D::hipBreak2(float force, bool blood)
{
    removeFromJointsToCheck(_hipJoint2);
    Node* parent = _upperLeg2Sprite->getParent();
    if (force <= _hipSnapLimit) {
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        b2FixtureDef fixtureDef;
        b2PolygonShape shape;
        b2Vec2 anchor = _hipJoint2->GetAnchorA();
        bodyDef.position = anchor;
        bodyDef.angle = _upperLeg2Body->GetAngle();
        fixtureDef.filter = _defaultFilter;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = 0.3f;
        fixtureDef.restitution = 0.1f;
        ValueMap legShape = _bodiesDict.at("bodies").asValueMap().at("sevLeg1Shape").asValueMap();
        Size size = SizeFromString(legShape.at("size").asString());
        Vec2 position = PointFromString(legShape.at("pos").asString());
        shape.SetAsBox(size.width, size.height, b2Vec2(0.0f, position.y), 0.0f);
        fixtureDef.shape = &shape;
        _upperLeg4Body = getWorld()->CreateBody(&bodyDef);
        _upperLeg4Body->CreateFixture(&fixtureDef);
        _upperLeg4Body->ResetMassData();
        _upperLeg4Body->SetLinearVelocity(_upperLeg2Body->GetLinearVelocity());
        _upperLeg4Body->SetAngularVelocity(_upperLeg2Body->GetAngularVelocity());

        getWorld()->DestroyJoint(_hipJoint2);
        _hipJoint2 = nullptr;
        b2RevoluteJointDef jointDef;
        float referenceAngle = _upperLeg4Body->GetAngle() - _pelvisBody->GetAngle();
        jointDef.enableLimit = true;
        jointDef.lowerAngle = -0.17453292f - referenceAngle;
        jointDef.upperAngle = 2.6179938f - referenceAngle;
        jointDef.maxMotorTorque = 4.0f;
        jointDef.Initialize(_pelvisBody, _upperLeg4Body, anchor);
        _hipJoint2 = (b2RevoluteJoint*)getWorld()->CreateJoint(&jointDef);

        ValueMap thighShape =
            _bodiesDict.at("bodies").asValueMap().at("sevLeg2Shape").asValueMap();
        Size thighSize = SizeFromString(thighShape.at("size").asString());
        Vec2 thighPosition = PointFromString(thighShape.at("pos").asString());
        // NOTE(sic, @005951dc): the zero filter is stored into the already used fixture definition
        // instead of the thigh fixture (no effect). Kept as in the binary.
        fixtureDef.filter = _zeroFilter;
        b2Fixture* fixture = _upperLeg2Body->GetFixtureList();
        ((b2PolygonShape*)fixture->GetShape())
            ->SetAsBox(thighSize.width, thighSize.height, b2Vec2(0.0f, thighPosition.y), 0.0f);
        _upperLeg2Body->ResetMassData();
        getLevel()->addFixtureMaterial(fixture, 1);
        fixture->SetUserData(this);
        _lowerLeg2Body->GetFixtureList()->SetFilterData(_zeroFilter);

        _upperLeg2Sprite->removeFromParentAndCleanup(false);
        if (_upperLeg2State == 5) {
            _upperLeg2State = 6;
            _upperLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg2_6.png");
        } else {
            _upperLeg2State = 3;
            _upperLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg2_3.png");
        }
        _upperLeg4Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg2_2.png");
        parent->addChild(_upperLeg2Sprite);
        parent->addChild(_upperLeg4Sprite);
        _upperLeg2Body->SetUserData(_upperLeg2Sprite);
        _upperLeg4Body->SetUserData(_upperLeg4Sprite);
        getLevel()->addToPaintBody(_upperLeg4Body);
        if (blood) {
            addHip2BloodFlow(force);
            createBodySound("LimbRip4", _upperLeg2Body, 1.0f, false);
        }
        addThigh2BloodFlow(force);
        if (_waistJoint) {
            addVocalsWithName("Hip2", VocalPriority5);
        }
    } else {
        getWorld()->DestroyJoint(_hipJoint2);
        _hipJoint2 = nullptr;
        b2Fixture* fixture = _upperLeg2Body->GetFixtureList();
        fixture->SetFilterData(_zeroFilter);
        getLevel()->addFixtureMaterial(fixture, 1);
        fixture->SetUserData(this);
        _lowerLeg2Body->GetFixtureList()->SetFilterData(_zeroFilter);

        _upperLeg2Sprite->removeFromParentAndCleanup(false);
        if (_upperLeg2State == 5) {
            _upperLeg2State = 7;
            _upperLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg2_7.png");
        } else {
            _upperLeg2State = 4;
            _upperLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg2_4.png");
        }
        parent->addChild(_upperLeg2Sprite);
        _upperLeg2Body->SetUserData(_upperLeg2Sprite);
        if (blood) {
            addHip2BloodFlow(force);
            createBodySound("LimbRip4", _upperLeg2Body, 1.0f, false);
        }
        addThigh2BloodFlow(force);
        if (_waistJoint) {
            addVocalsWithName("Hip2", VocalPriority5);
        }
    }
    if (_kneeJoint2) {
        float angle = (_lowerLeg2Body->GetAngle() - _upperLeg2Body->GetAngle()) -
                      owb2::jointAngle(_kneeJoint2);
        _kneeJoint2->SetLimits(-2.6179938f - angle, 0.0f - angle);
    }
    postInjury(CharacterInjuryHip2Break);
}

// @005961f4
// Shin torn off at the knee; below the ligament limit a ligament keeps it attached.
void CharacterB2D::kneeBreak1(float force)
{
    removeFromJointsToCheck(_kneeJoint1);
    Node* parent = _lowerLeg1Sprite->getParent();
    getWorld()->DestroyJoint(_kneeJoint1);
    _kneeJoint1 = nullptr;
    _lowerLeg1Body->GetFixtureList()->SetFilterData(_zeroFilter);

    int upperZOrder = _upperLeg1Sprite->getLocalZOrder();
    _upperLeg1Sprite->removeFromParentAndCleanup(false);
    int lowerZOrder = _lowerLeg1Sprite->getLocalZOrder();
    _lowerLeg1Sprite->removeFromParentAndCleanup(false);
    if (_lowerLeg1State == 3) {
        _lowerLeg1State = 4;
        _lowerLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerLeg1_4.png");
    } else {
        _lowerLeg1State = 2;
        _lowerLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerLeg1_2.png");
    }
    if (_upperLeg1State == 4) {
        _upperLeg1State = 7;
        _upperLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg1_7.png");
    } else if (_upperLeg1State == 3) {
        _upperLeg1State = 6;
        _upperLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg1_6.png");
    } else {
        _upperLeg1State = 5;
        _upperLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg1_5.png");
    }
    parent->addChild(_upperLeg1Sprite, upperZOrder);
    parent->addChild(_lowerLeg1Sprite, lowerZOrder);
    _upperLeg1Body->SetUserData(_upperLeg1Sprite);
    _lowerLeg1Body->SetUserData(_lowerLeg1Sprite);

    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        b2PolygonShape* shape = (b2PolygonShape*)_lowerLeg1Body->GetFixtureList()->GetShape();
        BurstEmitter* burst = BurstEmitter::createBloodBurst(
            5.0f, 15.0f, _lowerLeg1Body, b2Vec2(0.0f, shape->m_vertices[2].y), 50);
        if (burst) {
            particles->addChild(burst);
        }
    }
    if (force < _kneeLigamentLimit) {
        ValueMap ligamentShape =
            _bodiesDict.at("bodies").asValueMap().at("ligamentShape").asValueMap();
        Size size = SizeFromString(ligamentShape.at("size").asString());
        Ligament* ligament = Ligament::create(getPtm(), getSession()->getTimeStep(),
                                              _upperLeg1Body, _lowerLeg1Body, size.width);
        ligament->retain();
        _composites.push_back(ligament);
        getSession()->getMidgroundDrawNode()->addLigament(ligament);
    }
    int boneBreakNumber = ceilf((float)rand() * 4.656613e-10f * 4.0f);
    std::stringstream soundName;
    soundName << "BoneBreak" << boneBreakNumber;
    createBodySound(soundName.str(), _upperLeg1Body, 1.0f, false);
    if (_hipJoint1 && !_upperLeg3Body) {
        addVocalsWithName("Knee1", VocalPriority3);
    }
    postInjury(CharacterInjuryKnee1Break);
}

// @00596d98
// Mirror of kneeBreak1; additionally clears the sensor flag of the shin fixture, and re-adds the
// thigh one z-order below its previous one, after the shin.
void CharacterB2D::kneeBreak2(float force)
{
    removeFromJointsToCheck(_kneeJoint2);
    Node* parent = _lowerLeg2Sprite->getParent();
    getWorld()->DestroyJoint(_kneeJoint2);
    _kneeJoint2 = nullptr;
    b2Fixture* fixture = _lowerLeg2Body->GetFixtureList();
    fixture->SetFilterData(_zeroFilter);
    fixture->SetSensor(false);

    int upperZOrder = _upperLeg2Sprite->getLocalZOrder();
    _upperLeg2Sprite->removeFromParentAndCleanup(false);
    int lowerZOrder = _lowerLeg2Sprite->getLocalZOrder();
    _lowerLeg2Sprite->removeFromParentAndCleanup(false);
    if (_lowerLeg2State == 3) {
        _lowerLeg2State = 4;
        _lowerLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerLeg2_4.png");
    } else {
        _lowerLeg2State = 2;
        _lowerLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerLeg2_2.png");
    }
    if (_upperLeg2State == 4) {
        _upperLeg2State = 7;
        _upperLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg2_7.png");
    } else if (_upperLeg2State == 3) {
        _upperLeg2State = 6;
        _upperLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg2_6.png");
    } else {
        _upperLeg2State = 5;
        _upperLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg2_5.png");
    }
    parent->addChild(_lowerLeg2Sprite, lowerZOrder);
    parent->addChild(_upperLeg2Sprite, upperZOrder - 1);
    _upperLeg2Body->SetUserData(_upperLeg2Sprite);
    _lowerLeg2Body->SetUserData(_lowerLeg2Sprite);

    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        b2PolygonShape* shape = (b2PolygonShape*)_lowerLeg2Body->GetFixtureList()->GetShape();
        BurstEmitter* burst = BurstEmitter::createBloodBurst(
            5.0f, 15.0f, _lowerLeg2Body, b2Vec2(0.0f, shape->m_vertices[2].y), 50);
        if (burst) {
            particles->addChild(burst);
        }
    }
    if (force < _kneeLigamentLimit) {
        ValueMap ligamentShape =
            _bodiesDict.at("bodies").asValueMap().at("ligamentShape").asValueMap();
        Size size = SizeFromString(ligamentShape.at("size").asString());
        Ligament* ligament = Ligament::create(getPtm(), getSession()->getTimeStep(),
                                              _upperLeg2Body, _lowerLeg2Body, size.width);
        ligament->retain();
        _composites.push_back(ligament);
        getSession()->getBackgroundDrawNode()->addLigament(ligament);
    }
    int boneBreakNumber = ceilf((float)rand() * 4.656613e-10f * 4.0f);
    std::stringstream soundName;
    soundName << "BoneBreak" << boneBreakNumber;
    createBodySound(soundName.str(), _upperLeg2Body, 1.0f, false);
    if (_hipJoint2 && !_upperLeg4Body) {
        addVocalsWithName("Knee2", VocalPriority3);
    }
    postInjury(CharacterInjuryKnee2Break);
}

// @0059794c
void CharacterB2D::handleContactAdds()
{
}

// @00597950
// Knocks the helmet off: it becomes its own body with the head's position and velocity, and
// the head gets its normal smash threshold back.
void CharacterB2D::helmetSmash(float impulse)
{
    removeFromContactResultBufferDict(_headFixture);
    _contactImpulseDict[_headFixture] = _headSmashLimit;
    Node* parent = _headSprite->getParent();
    Node* helmet = _headSprite->getChildByTag(0);
    if (helmet) {
        helmet->removeFromParentAndCleanup(false);
    }
    Sprite* helmetSprite = createHelmetSprite();
    parent->addChild(helmetSprite);

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = _headBody->GetPosition();
    bodyDef.angle = _headBody->GetAngle();
    bodyDef.userData = helmetSprite;
    _helmetBody = getWorld()->CreateBody(&bodyDef);
    _helmetBody->SetAngularVelocity(_headBody->GetAngularVelocity());
    _helmetBody->SetLinearVelocity(_headBody->GetLinearVelocity());
    ValueMap bodies = _bodiesDict.at("bodies").asValueMap();
    createFixture(_helmetBody, _zeroFilter, &bodies.at("helmetShape").asValueMap());
    _helmetBody->ResetMassData();
    getLevel()->addToPaintBody(_helmetBody);
}

// @00597dd8
// The head bursts: with gore it is replaced by four "headChunk" bodies placed around the head
// centre and a brain body; the head body is destroyed.
void CharacterB2D::headSmash(float impulse)
{
    removePostSolve(_headFixture);
    removeFromContactResultBufferDict(_headFixture);
    if (_helmetOn && !_helmetBody) {
        helmetSmash(impulse);
    }
    setDead(true);
    if (_showGore) {
        Node* parent = _headSprite->getParent();
        _headSprite->removeFromParentAndCleanup(false);
        _headSprite = nullptr;
        EmitterNode* particles = getSession()->getParticlesForeground();
        if (particles) {
            BurstEmitter* burst =
                BurstEmitter::createBloodBurst(5.0f, 15.0f, _headBody, b2Vec2_zero, 200);
            if (burst) {
                particles->addChild(burst);
            }
        }
        if (_neckJoint) {
            getWorld()->DestroyJoint(_neckJoint);
            removeFromJointsToCheck(_neckJoint);
            _neckJoint = nullptr;
            addNeckWoundToChest();
            addNeckBloodFlow();
        } else {
            if (_headBloodFlow) {
                _headBloodFlow->stop();
                _headBloodFlow->removeFromParent();
                _headBloodFlow = nullptr;
            }
            if (_spinalCord) {
                _spinalCord->spineBreak2();
            }
        }

        b2FixtureDef fixtureDef;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = 0.3f;
        fixtureDef.restitution = 0.1f;
        b2CircleShape circleShape;
        circleShape.m_radius = _bodiesDict.at("bodies")
                                   .asValueMap()
                                   .at("headChunkShape")
                                   .asValueMap()
                                   .at("radius")
                                   .asFloat();
        fixtureDef.shape = &circleShape;
        fixtureDef.filter = _zeroFilter;

        b2Vec2 headPosition = _headBody->GetPosition();
        float angle = _headBody->GetAngle();
        b2Vec2 headVelocity = _headBody->GetLinearVelocity();
        float headAngularVelocity = _headBody->GetAngularVelocity();

        Sprite* brainSprite = Sprite::createWithSpriteFrameName(_name + "_brain.png");
        brainSprite->setAnchorPoint(_brainAnchor);
        parent->addChild(brainSprite);

        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.angle = angle;
        b2Body* chunkBody = nullptr;
        for (unsigned int i = 1; i < 5; i++) {
            std::stringstream number;
            number << i;
            Sprite* chunkSprite =
                Sprite::createWithSpriteFrameName(_name + "_headChunk_" + number.str() + ".png");
            parent->addChild(chunkSprite);
            bodyDef.userData = chunkSprite;
            bodyDef.position.Set(headPosition.x + sinf(angle) * 0.032f,
                                 headPosition.y + cosf(angle) * 0.032f);
            chunkBody = getWorld()->CreateBody(&bodyDef);
            getLevel()->addToPaintBody(chunkBody);
            chunkBody->CreateFixture(&fixtureDef);
            chunkBody->ResetMassData();
            chunkBody->SetLinearVelocity(_headBody->GetLinearVelocityFromWorldPoint(bodyDef.position));
            chunkBody->SetAngularVelocity(headAngularVelocity);
            angle += 1.5707963267948966;
        }

        getLevel()->removeFromPaintBody(_headBody);
        getWorld()->DestroyBody(_headBody);
        _headBody = nullptr;

        circleShape.m_radius = 0.096f;
        b2BodyDef brainDef;
        brainDef.type = b2_dynamicBody;
        brainDef.position = headPosition;
        brainDef.angle = angle;
        brainDef.userData = brainSprite;
        _brainBody = getWorld()->CreateBody(&brainDef);
        getLevel()->addToPaintBody(_brainBody);
        fixtureDef.shape = &circleShape;
        fixtureDef.filter = _zeroFilter;
        _brainBody->CreateFixture(&fixtureDef);
        _brainBody->ResetMassData();
        _brainBody->SetLinearVelocity(headVelocity);
        _brainBody->SetAngularVelocity(headAngularVelocity);
        createBodySound("HeadSmash", chunkBody, 1.0f, false);
    }
    postInjury(CharacterInjuryHeadSmash);
}

// @00598a88
// The chest bursts: with gore every remaining joint around the chest is broken, the chest is
// replaced by four "chestChunk" bodies and a heart body (the new camera focus), and the chest
// body is destroyed.
void CharacterB2D::chestSmash(float impulse)
{
    removeFromContactResultBufferDict(_chestFixture);
    removePostSolve(_chestFixture);
    setDead(true);
    if (_showGore) {
        Node* parent = _chestSprite->getParent();
        if (_neckJoint) {
            removeFromJointsToCheck(_neckJoint);
            neckBreak(_spineLimit, false, false);
        } else if (_spinalCord) {
            _spinalCord->spineBreak1();
        }
        if (_shoulderJoint1) {
            if (_upperArm3Body) {
                // NOTE(sic, @00598c20): the joint pointer is left dangling and the old sprite of
                // the dislocated arm stays in place (no removal, no new body user data).
                getWorld()->DestroyJoint(_shoulderJoint1);
                _upperArm3Body->GetFixtureList()->SetFilterData(_zeroFilter);
                int zOrder = _upperArm3Sprite->getLocalZOrder();
                _upperArm3Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm1_8.png");
                parent->addChild(_upperArm3Sprite, zOrder);
            } else {
                removeFromJointsToCheck(_shoulderJoint1);
                shoulderBreak1(_shoulderSnapLimit + 1.0f, false);
            }
        }
        if (_shoulderJoint2) {
            if (_upperArm4Body) {
                getWorld()->DestroyJoint(_shoulderJoint2);
                _upperArm4Body->GetFixtureList()->SetFilterData(_zeroFilter);
                int zOrder = _upperArm4Sprite->getLocalZOrder();
                _upperArm4Sprite = Sprite::createWithSpriteFrameName(_name + "_upperArm2_8.png");
                parent->addChild(_upperArm4Sprite, zOrder);
            } else {
                removeFromJointsToCheck(_shoulderJoint2);
                shoulderBreak2(_shoulderSnapLimit + 1.0f, false);
            }
        }
        if (_waistJoint) {
            removeFromJointsToCheck(_waistJoint);
            torsoBreak(_intestineLimit + -1.0f, false, true, false);
        }
        if (_intestineChain) {
            _intestineChain->intestineBreak2();
        }

        b2FixtureDef fixtureDef;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = 0.3f;
        fixtureDef.restitution = 0.1f;
        b2CircleShape circleShape;
        circleShape.m_radius = _bodiesDict.at("bodies")
                                   .asValueMap()
                                   .at("chestChunkShape")
                                   .asValueMap()
                                   .at("radius")
                                   .asFloat();
        fixtureDef.shape = &circleShape;
        fixtureDef.filter = _zeroFilter;

        b2Vec2 chestPosition = _chestBody->GetPosition();
        float angle = _chestBody->GetAngle() + 0.7853981633974483;

        Sprite* heartSprite = Sprite::createWithSpriteFrameName(_name + "_heart.png");
        heartSprite->setAnchorPoint(_heartAnchor);
        parent->addChild(heartSprite);

        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        b2Body* chunkBody = nullptr;
        for (unsigned int i = 1; i < 5; i++) {
            std::stringstream number;
            number << i;
            Sprite* chunkSprite =
                Sprite::createWithSpriteFrameName(_name + "_chestChunk_" + number.str() + ".png");
            parent->addChild(chunkSprite);
            bodyDef.userData = chunkSprite;
            bodyDef.position.Set(chestPosition.x + sinf(angle) * 0.04f,
                                 chestPosition.y + cosf(angle) * 0.04f);
            bodyDef.angle = _chestBody->GetAngle();
            chunkBody = getWorld()->CreateBody(&bodyDef);
            getLevel()->addToPaintBody(chunkBody);
            chunkBody->CreateFixture(&fixtureDef);
            chunkBody->ResetMassData();
            chunkBody->SetLinearVelocity(
                _chestBody->GetLinearVelocityFromWorldPoint(bodyDef.position));
            chunkBody->SetAngularVelocity(_chestBody->GetAngularVelocity());
            angle += 1.5707963267948966;
        }

        circleShape.m_radius = _bodiesDict.at("bodies")
                                   .asValueMap()
                                   .at("headChunkShape")
                                   .asValueMap()
                                   .at("radius")
                                   .asFloat();
        b2BodyDef heartDef;
        heartDef.type = b2_dynamicBody;
        heartDef.position = chestPosition;
        heartDef.angle = _chestBody->GetAngle();
        heartDef.userData = heartSprite;
        _heartBody = getWorld()->CreateBody(&heartDef);
        fixtureDef.shape = &circleShape;
        _heartBody->CreateFixture(&fixtureDef);
        _heartBody->ResetMassData();
        _heartBody->SetLinearVelocity(_chestBody->GetLinearVelocity());
        _heartBody->SetAngularVelocity(_chestBody->GetAngularVelocity());
        getLevel()->addToPaintBody(_heartBody);

        if (_stomachBloodFlow) {
            _stomachBloodFlow->stop();
            _stomachBloodFlow->removeFromParent();
            _stomachBloodFlow = nullptr;
        }
        if (_neckBloodFlow) {
            _neckBloodFlow->stop();
            _neckBloodFlow->removeFromParent();
            _neckBloodFlow = nullptr;
        }
        if (_shoulder1BloodFlow) {
            _shoulder1BloodFlow->stop();
            _shoulder1BloodFlow->removeFromParent();
            _shoulder1BloodFlow = nullptr;
        }
        if (_shoulder2BloodFlow) {
            _shoulder2BloodFlow->stop();
            _shoulder2BloodFlow->removeFromParent();
            _shoulder2BloodFlow = nullptr;
        }
        EmitterNode* particles = getSession()->getParticlesForeground();
        if (particles) {
            BurstEmitter* burst =
                BurstEmitter::createBloodBurst(5.0f, 20.0f, _chestBody, b2Vec2_zero, 300);
            if (burst) {
                particles->addChild(burst);
            }
        }
        createBodySound("ChestSmash", chunkBody, 1.0f, false);
        getLevel()->removeFromPaintBody(_chestBody);
        getWorld()->DestroyBody(_chestBody);
        _chestSprite->removeFromParentAndCleanup(false);
        _chestBody = nullptr;
        setFocus(_heartBody);
    }
    postInjury(CharacterInjuryChestSmash);
}

// @00599ca4
// The pelvis bursts: with gore both hips and the waist are broken, the pelvis is replaced by
// three "pelvisChunk" bodies and the pelvis body is destroyed. (Does not kill by itself.)
void CharacterB2D::pelvisSmash(float impulse)
{
    removeFromContactResultBufferDict(_pelvisFixture);
    removePostSolve(_pelvisFixture);
    Node* parent = _pelvisSprite->getParent();
    if (_showGore) {
        if (_hipJoint1) {
            if (_upperLeg3Body) {
                getWorld()->DestroyJoint(_hipJoint1);
                _hipJoint1 = nullptr;
                _upperLeg3Body->GetFixtureList()->SetFilterData(_zeroFilter);
                _upperLeg3Sprite->removeFromParentAndCleanup(false);
                _upperLeg3Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg1_8.png");
                parent->addChild(_upperLeg3Sprite);
                _upperLeg3Body->SetUserData(_upperLeg3Sprite);
            } else {
                removeFromJointsToCheck(_hipJoint1);
                hipBreak1(_hipSnapLimit + 1.0f, false);
            }
        }
        if (_hipJoint2) {
            if (_upperLeg4Body) {
                // NOTE(sic, @00599f0c): unlike hip 1 the joint pointer is not cleared.
                getWorld()->DestroyJoint(_hipJoint2);
                _upperLeg4Body->GetFixtureList()->SetFilterData(_zeroFilter);
                _upperLeg4Sprite->removeFromParentAndCleanup(false);
                _upperLeg4Sprite = Sprite::createWithSpriteFrameName(_name + "_upperLeg2_8.png");
                parent->addChild(_upperLeg4Sprite);
                _upperLeg4Body->SetUserData(_upperLeg4Sprite);
            } else {
                removeFromJointsToCheck(_hipJoint2);
                hipBreak2(_hipSnapLimit + 1.0f, false);
            }
        }
        if (_waistJoint) {
            removeFromJointsToCheck(_waistJoint);
            torsoBreak(_intestineLimit, true, true, false);
        } else if (_intestineChain) {
            _intestineChain->intestineBreak1();
        }
        if (_hip1BloodFlow) {
            _hip1BloodFlow->stop();
            _hip1BloodFlow->removeFromParent();
            _hip1BloodFlow = nullptr;
        }
        if (_hip2BloodFlow) {
            _hip2BloodFlow->stop();
            _hip2BloodFlow->removeFromParent();
            _hip2BloodFlow = nullptr;
        }

        b2FixtureDef fixtureDef;
        fixtureDef.density = 1.0f;
        fixtureDef.friction = 0.3f;
        fixtureDef.restitution = 0.1f;
        b2CircleShape circleShape;
        circleShape.m_radius = _bodiesDict.at("bodies")
                                   .asValueMap()
                                   .at("pelvisChunkShape")
                                   .asValueMap()
                                   .at("radius")
                                   .asFloat();
        fixtureDef.shape = &circleShape;
        fixtureDef.filter = _zeroFilter;

        b2Vec2 pelvisPosition = _pelvisBody->GetPosition();
        float angle = _pelvisBody->GetAngle();
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.angle = angle;
        b2Body* chunkBody = nullptr;
        for (unsigned int i = 1; i < 4; i++) {
            std::stringstream number;
            number << i;
            Sprite* chunkSprite =
                Sprite::createWithSpriteFrameName(_name + "_pelvisChunk_" + number.str() + ".png");
            parent->addChild(chunkSprite);
            bodyDef.userData = chunkSprite;
            bodyDef.position.Set(pelvisPosition.x + sinf(angle) * 0.024f,
                                 pelvisPosition.y + cosf(angle) * 0.024f);
            chunkBody = getWorld()->CreateBody(&bodyDef);
            chunkBody->CreateFixture(&fixtureDef);
            chunkBody->ResetMassData();
            chunkBody->SetLinearVelocity(
                _pelvisBody->GetLinearVelocityFromWorldPoint(bodyDef.position));
            chunkBody->SetAngularVelocity(_pelvisBody->GetAngularVelocity());
            getLevel()->addToPaintBody(chunkBody);
            angle += 2.0943951023931953;
        }

        EmitterNode* particles = getSession()->getParticlesForeground();
        if (particles && _pelvisBody) {
            BurstEmitter* burst =
                BurstEmitter::createBloodBurst(5.0f, 15.0f, _pelvisBody, b2Vec2_zero, 200);
            if (burst) {
                particles->addChild(burst);
            }
        }
        createBodySound("PelvisSmash", chunkBody, 1.0f, false);
        getWorld()->DestroyBody(_pelvisBody);
        getLevel()->removeFromPaintBody(_pelvisBody);
        _pelvisBody = nullptr;
        _pelvisSprite->removeFromParentAndCleanup(false);
        _pelvisSprite = nullptr;
        if (_waistJoint) {
            addVocalsWithName("Pelvis", VocalPriority6);
        }
    }
    postInjury(CharacterInjuryPelvisSmash);
}

// @0059aa90
// The foot breaks off: a separate foot body ("footShape", placed at the shape position on the
// shin) gets the shin's velocity, and the shin sprite shows the stump.
void CharacterB2D::foot1Smash(float impulse)
{
    removePostSolve(_lowerLeg1Fixture);
    removeFromContactResultBufferDict(_lowerLeg1Fixture);
    if (_showGore) {
        Node* parent = _lowerLeg1Sprite->getParent();
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        b2PolygonShape shape;
        b2FixtureDef fixtureDef;
        ValueMap footShape = _bodiesDict.at("bodies").asValueMap().at("footShape").asValueMap();
        Size size = SizeFromString(footShape.at("size").asString());
        Vec2 position = PointFromString(footShape.at("pos").asString());
        shape.SetAsBox(size.width, size.height);
        bodyDef.position = _lowerLeg1Body->GetWorldPoint(b2Vec2(position.x, position.y));
        bodyDef.angle = _lowerLeg1Body->GetAngle();
        _foot1Sprite = Sprite::createWithSpriteFrameName(_name + "_foot.png");
        parent->addChild(_foot1Sprite);
        bodyDef.userData = _foot1Sprite;
        fixtureDef.filter = _zeroFilter;
        fixtureDef.friction = 0.3f;
        fixtureDef.restitution = 0.1f;
        fixtureDef.density = 1.0f;
        fixtureDef.shape = &shape;
        b2Body* footBody = getWorld()->CreateBody(&bodyDef);
        getLevel()->addToPaintBody(footBody);
        footBody->CreateFixture(&fixtureDef);
        footBody->ResetMassData();
        footBody->SetLinearVelocity(_lowerLeg1Body->GetLinearVelocity());
        footBody->SetAngularVelocity(_lowerLeg1Body->GetAngularVelocity());

        EmitterNode* particles = getSession()->getParticlesForeground();
        if (particles) {
            b2PolygonShape* legShape = (b2PolygonShape*)_lowerLeg1Body->GetFixtureList()->GetShape();
            BurstEmitter* burst = BurstEmitter::createBloodBurst(
                5.0f, 15.0f, _lowerLeg1Body, b2Vec2(0.0f, legShape->m_vertices[0].y), 50);
            if (burst) {
                particles->addChild(burst);
            }
        }

        int zOrder = _lowerLeg1Sprite->getLocalZOrder();
        _lowerLeg1Sprite->removeFromParentAndCleanup(false);
        if (_lowerLeg1State == 2) {
            _lowerLeg1State = 4;
            _lowerLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerLeg1_4.png");
        } else {
            _lowerLeg1State = 3;
            _lowerLeg1Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerLeg1_3.png");
        }
        parent->addChild(_lowerLeg1Sprite, zOrder - 1);
        _lowerLeg1Body->SetUserData(_lowerLeg1Sprite);

        int boneBreakNumber = ceilf((float)rand() * 4.656613e-10f * 4.0f);
        std::stringstream soundName;
        soundName << "BoneBreak" << boneBreakNumber;
        createBodySound(soundName.str(), _lowerLeg1Body, 1.0f, false);
        if (_kneeJoint1 && !_upperLeg3Body) {
            addVocalsWithName("Foot1", VocalPriority1);
        }
    }
    postInjury(CharacterInjuryFoot1Smash);
}

// @0059b580
// Mirror of foot1Smash (blood burst in the particle midground).
void CharacterB2D::foot2Smash(float impulse)
{
    removePostSolve(_lowerLeg2Fixture);
    removeFromContactResultBufferDict(_lowerLeg2Fixture);
    if (_showGore) {
        Node* parent = _lowerLeg2Sprite->getParent();
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        b2PolygonShape shape;
        b2FixtureDef fixtureDef;
        ValueMap footShape = _bodiesDict.at("bodies").asValueMap().at("footShape").asValueMap();
        Size size = SizeFromString(footShape.at("size").asString());
        Vec2 position = PointFromString(footShape.at("pos").asString());
        shape.SetAsBox(size.width, size.height);
        bodyDef.position = _lowerLeg2Body->GetWorldPoint(b2Vec2(position.x, position.y));
        bodyDef.angle = _lowerLeg2Body->GetAngle();
        _foot2Sprite = Sprite::createWithSpriteFrameName(_name + "_foot.png");
        parent->addChild(_foot2Sprite);
        bodyDef.userData = _foot2Sprite;
        fixtureDef.filter = _zeroFilter;
        fixtureDef.friction = 0.3f;
        fixtureDef.restitution = 0.1f;
        fixtureDef.density = 1.0f;
        fixtureDef.shape = &shape;
        b2Body* footBody = getWorld()->CreateBody(&bodyDef);
        getLevel()->addToPaintBody(footBody);
        footBody->CreateFixture(&fixtureDef);
        footBody->ResetMassData();
        footBody->SetLinearVelocity(_lowerLeg2Body->GetLinearVelocity());
        footBody->SetAngularVelocity(_lowerLeg2Body->GetAngularVelocity());

        EmitterNode* particles = getSession()->getParticlesMidground();
        if (particles) {
            b2PolygonShape* legShape = (b2PolygonShape*)_lowerLeg2Body->GetFixtureList()->GetShape();
            BurstEmitter* burst = BurstEmitter::createBloodBurst(
                5.0f, 15.0f, _lowerLeg2Body, b2Vec2(0.0f, legShape->m_vertices[0].y), 50);
            if (burst) {
                particles->addChild(burst);
            }
        }

        int zOrder = _lowerLeg2Sprite->getLocalZOrder();
        _lowerLeg2Sprite->removeFromParentAndCleanup(false);
        if (_lowerLeg2State == 2) {
            _lowerLeg2State = 4;
            _lowerLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerLeg2_4.png");
        } else {
            _lowerLeg2State = 3;
            _lowerLeg2Sprite = Sprite::createWithSpriteFrameName(_name + "_lowerLeg2_3.png");
        }
        parent->addChild(_lowerLeg2Sprite, zOrder - 1);
        _lowerLeg2Body->SetUserData(_lowerLeg2Sprite);

        int boneBreakNumber = ceilf((float)rand() * 4.656613e-10f * 4.0f);
        std::stringstream soundName;
        soundName << "BoneBreak" << boneBreakNumber;
        createBodySound(soundName.str(), _lowerLeg2Body, 1.0f, false);
        if (_kneeJoint2 && !_upperLeg4Body) {
            addVocalsWithName("Foot2", VocalPriority1);
        }
    }
    postInjury(CharacterInjuryFoot2Smash);
}

// ---------------------------------------------------------------------------------------------
// Grabbing, death, ejection
// ---------------------------------------------------------------------------------------------

// @0059c070
// Hand 1 touched something while grabbing: pin it there with a revolute joint at the fingertips.
void CharacterB2D::grabAction1(b2Body* otherBody)
{
    b2RevoluteJointDef jointDef;
    b2PolygonShape* shape = (b2PolygonShape*)_lowerArm1Fixture->GetShape();
    b2Vec2 anchor = _lowerArm1Body->GetWorldPoint(b2Vec2(0.0f, -shape->m_vertices[2].y));
    jointDef.enableLimit = otherBody->GetType() != b2_staticBody;
    jointDef.maxMotorTorque = 4.0f;
    jointDef.Initialize(otherBody, _lowerArm1Body, anchor);
    openHand1(false);
    _gripJoint1 = (b2RevoluteJoint*)getWorld()->CreateJoint(&jointDef);
    getSession()->getDestructionListener()->addJointListener(_gripJoint1, this);
    removePostSolve(_lowerArm1Fixture);
}

// @0059c1f8
void CharacterB2D::grabAction2(b2Body* otherBody)
{
    b2RevoluteJointDef jointDef;
    b2PolygonShape* shape = (b2PolygonShape*)_lowerArm2Fixture->GetShape();
    b2Vec2 anchor = _lowerArm2Body->GetWorldPoint(b2Vec2(0.0f, -shape->m_vertices[2].y));
    jointDef.enableLimit = otherBody->GetType() != b2_staticBody;
    jointDef.maxMotorTorque = 4.0f;
    jointDef.Initialize(otherBody, _lowerArm2Body, anchor);
    openHand2(false);
    _gripJoint2 = (b2RevoluteJoint*)getWorld()->CreateJoint(&jointDef);
    getSession()->getDestructionListener()->addJointListener(_gripJoint2, this);
    removePostSolve(_lowerArm2Fixture);
}

// @0059c380
void CharacterB2D::setDead(bool dead)
{
    if (_dead != dead && (_dead = dead)) {
        eject();
        if (_mainCharacter) {
            Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("characterDead");
        }
        if (_mourner) {
            _mourner->mourn();
        }
        postInjury(CharacterInjuryDeath);
        setCurrentPose(CharacterPoseNone);
        endGrab();
        if (_voiceSound) {
            _voiceSound->stop();
            _voiceSound = nullptr;
        }
    }
}

// @0059c4cc
// Separates the character from its vehicle (the vehicle side is handled by Vehicle).
void CharacterB2D::eject()
{
    if (!_ejected) {
        _ejected = true;
        _qolEjectedTime = 0.0f;  // QOL (PC addition): re-grab vehicle
        openHand1(true);
        openHand2(true);
        resetJointLimits();
        if (_mainCharacter) {
            Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("characterEjected");
        }
    }
}

// @0059c6a8
void CharacterB2D::postInjury(CharacterInjury injury)
{
    // QOL (PC addition): re-grab vehicle - a re-mount lets go of the same limbs again.
    if (std::find(_qolInjuries.begin(), _qolInjuries.end(), injury) == _qolInjuries.end()) {
        _qolInjuries.push_back(injury);
    }
    // ONLINE (PC addition): a lost arm / death lets go of a browser user vehicle.
    if (online::flashLevel()) {
        onlineUserVehicleInjury(injury);
    }
    if (_vehicle) {
        _vehicle->handleInjury(injury, this);
    }
}

// @0059c6c4
void CharacterB2D::mourn()
{
    addVocalsWithName("Mourn", VocalPriority1);
}

// @0059c774
void CharacterB2D::addVocalsWithName(std::string name, VocalPriority priority)
{
    _nextVocalPriority = priority;
    _nextVocalString = name;
}

// @0059c784
void CharacterB2D::setDying(bool dying)
{
    if (_mainCharacter) {
        Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("characterDying");
    }
    _dying = dying;
}

// @0059c858
// Joint break forces are measured as impulse * 1/step, so the limits follow the step change.
void CharacterB2D::timeStepChanged()
{
    if (_intestineChain) {
        _intestineChain->setTimeStep(LevelItem::s_timeStep);
    }
    if (_spinalCord) {
        _spinalCord->setTimeStep(LevelItem::s_timeStep);
    }
    float ratio = getTimeStepInverse() / (1.0f / getPreviousTimeStep());
    _neckBreakLimit = _neckBreakLimit * ratio;
    _jointLimits[_neckJoint] = _neckBreakLimit;
    _shoulderBreakLimit = ratio * _shoulderBreakLimit;
    _jointLimits[_shoulderJoint2] = _shoulderBreakLimit;
    _jointLimits[_shoulderJoint1] = _shoulderBreakLimit;
    _hipBreakLimit = ratio * _hipBreakLimit;
    _jointLimits[_hipJoint2] = _hipBreakLimit;
    _jointLimits[_hipJoint1] = _hipBreakLimit;
    _elbowBreakLimit = ratio * _elbowBreakLimit;
    _jointLimits[_elbowJoint2] = _elbowBreakLimit;
    _jointLimits[_elbowJoint1] = _elbowBreakLimit;
    _kneeBreakLimit = ratio * _kneeBreakLimit;
    _jointLimits[_kneeJoint2] = _kneeBreakLimit;
    _jointLimits[_kneeJoint1] = _kneeBreakLimit;
    _torsoBreakLimit = ratio * _torsoBreakLimit;
    _jointLimits[_waistJoint] = _torsoBreakLimit;
    _spineLimit = ratio * _spineLimit;
    _intestineLimit = ratio * _intestineLimit;
    _shoulderSnapLimit = ratio * _shoulderSnapLimit;
    _hipSnapLimit = ratio * _hipSnapLimit;
    _elbowLigamentLimit = ratio * _elbowLigamentLimit;
    // NOTE(sic, @0059cf64): the binary scales the knee break limit (+0x4b0) a second time here
    // and never the knee ligament limit (+0x4b4). Kept as in the binary.
    _kneeBreakLimit = ratio * _kneeBreakLimit;
}

// ---------------------------------------------------------------------------------------------
// Shapes and joint limits
// ---------------------------------------------------------------------------------------------

// @0059cfac
void CharacterB2D::shortenRectOfFixture(b2Fixture* fixture, float percentage, bool offTheTop)
{
}

// @0059cfb0
// Narrows the limb boxes at their lower end (vertices 0 and 1).
void CharacterB2D::taperBodies()
{
    taperBody(_upperArm1Body, 0.8f, false);
    taperBody(_upperArm2Body, 0.8f, false);
    taperBody(_lowerArm1Body, 0.65f, false);
    taperBody(_lowerArm2Body, 0.65f, false);
    taperBody(_upperLeg1Body, 0.75f, false);
    taperBody(_upperLeg2Body, 0.75f, false);
    taperBody(_lowerLeg1Body, 0.7f, false);
    taperBody(_lowerLeg2Body, 0.7f, false);
    _upperArm1Body->ResetMassData();
    _upperArm2Body->ResetMassData();
    _lowerArm1Body->ResetMassData();
    _lowerArm2Body->ResetMassData();
    _upperLeg1Body->ResetMassData();
    _upperLeg2Body->ResetMassData();
    _lowerLeg1Body->ResetMassData();
    _lowerLeg2Body->ResetMassData();
}

// @0059d14c
void CharacterB2D::taperBody(b2Body* body, float percent, bool top)
{
    b2PolygonShape* shape = (b2PolygonShape*)body->GetFixtureList()->GetShape();
    if (top) {
        shape->m_vertices[2].x *= percent;
        shape->m_vertices[3].x *= percent;
    } else {
        shape->m_vertices[0].x *= percent;
        shape->m_vertices[1].x *= percent;
    }
}

// @0059d18c
// Swaps the closed/open hand sprite of arm 1 (keeping position and rotation).
void CharacterB2D::openHand1(bool open)
{
    if (_elbowJoint1) {
        Sprite* shown = open ? _lowerArmOpen1Sprite : _lowerArm1Sprite;
        Sprite* hidden = open ? _lowerArm1Sprite : _lowerArmOpen1Sprite;
        hidden->setVisible(false);
        shown->setVisible(true);
        shown->setPosition(hidden->getPosition());
        shown->setRotation(hidden->getRotation());
        _lowerArm1Body->SetUserData(shown);
    }
}

// @0059d23c
void CharacterB2D::openHand2(bool open)
{
    if (_elbowJoint2) {
        Sprite* shown = open ? _lowerArmOpen2Sprite : _lowerArm2Sprite;
        Sprite* hidden = open ? _lowerArm2Sprite : _lowerArmOpen2Sprite;
        hidden->setVisible(false);
        shown->setVisible(true);
        shown->setPosition(hidden->getPosition());
        shown->setRotation(hidden->getRotation());
        _lowerArm2Body->SetUserData(shown);
    }
}

// @0059d2ec
// Re-applies the rest limits (radians) relative to each joint's current reference angle.
void CharacterB2D::resetJointLimits()
{
    if (_neckJoint) {
        float angle = (_headBody->GetAngle() - _chestBody->GetAngle()) - owb2::jointAngle(_neckJoint);
        _neckJoint->SetLimits(-0.34906584f - angle, 0.34906584f - angle);
    }
    if (_waistJoint) {
        float angle =
            (_pelvisBody->GetAngle() - _chestBody->GetAngle()) - owb2::jointAngle(_waistJoint);
        _waistJoint->SetLimits(-0.08726646f - angle, 0.08726646f - angle);
    }
    if (_shoulderJoint1) {
        float angle = (_shoulderJoint1->GetBodyB()->GetAngle() -
                       _shoulderJoint1->GetBodyA()->GetAngle()) -
                      owb2::jointAngle(_shoulderJoint1);
        _shoulderJoint1->SetLimits(-1.0471976f - angle, 3.1415925f - angle);
    }
    if (_shoulderJoint2) {
        float angle = (_shoulderJoint2->GetBodyB()->GetAngle() -
                       _shoulderJoint2->GetBodyA()->GetAngle()) -
                      owb2::jointAngle(_shoulderJoint2);
        _shoulderJoint2->SetLimits(-1.0471976f - angle, 3.1415925f - angle);
    }
    if (_elbowJoint1) {
        float angle = (_lowerArm1Body->GetAngle() - _upperArm1Body->GetAngle()) -
                      owb2::jointAngle(_elbowJoint1);
        _elbowJoint1->SetLimits(0.0f - angle, 2.7925267f - angle);
    }
    if (_elbowJoint2) {
        float angle = (_lowerArm2Body->GetAngle() - _upperArm2Body->GetAngle()) -
                      owb2::jointAngle(_elbowJoint2);
        _elbowJoint2->SetLimits(0.0f - angle, 2.7925267f - angle);
    }
    if (_hipJoint1) {
        float angle =
            (_hipJoint1->GetBodyB()->GetAngle() - _hipJoint1->GetBodyA()->GetAngle()) -
            owb2::jointAngle(_hipJoint1);
        _hipJoint1->SetLimits(-0.17453292f - angle, 2.6179938f - angle);
    }
    if (_hipJoint2) {
        float angle =
            (_hipJoint2->GetBodyB()->GetAngle() - _hipJoint2->GetBodyA()->GetAngle()) -
            owb2::jointAngle(_hipJoint2);
        _hipJoint2->SetLimits(-0.17453292f - angle, 2.6179938f - angle);
    }
    if (_kneeJoint1) {
        float angle = (_lowerLeg1Body->GetAngle() - _upperLeg1Body->GetAngle()) -
                      owb2::jointAngle(_kneeJoint1);
        _kneeJoint1->SetLimits(-2.6179938f - angle, 0.0f - angle);
    }
    if (_kneeJoint2) {
        float angle = (_lowerLeg2Body->GetAngle() - _upperLeg2Body->GetAngle()) -
                      owb2::jointAngle(_kneeJoint2);
        _kneeJoint2->SetLimits(-2.6179938f - angle, 0.0f - angle);
    }
}

// @0059d5e0
// Impaled (spikes, arrows...): a fatal hit on the head kills when it lands within killDistance
// (squared) of the head centre, or when the position is unknown (infinite).
int CharacterB2D::shapeImpale(b2Fixture* fixture, bool fatal, b2Vec2 stabPosition,
                              float killDistance)
{
    if (fixture == _chestFixture || fixture == _pelvisFixture) {
        playRandomVocals(fixture, VocalPriority5);
    } else if (fixture == _headFixture) {
        if (fatal && (stabPosition.x == INFINITY || stabPosition.y == INFINITY ||
                      (stabPosition - fixture->GetBody()->GetWorldCenter()).LengthSquared() <
                          killDistance)) {
            eject();
            setDead(true);
        } else {
            playRandomVocals(fixture, VocalPriority5);
        }
    }
    return getFluidType();
}

// @0059d694
// Queues one of the _randomVocals clips, picked from the fixture's x position.
void CharacterB2D::playRandomVocals(b2Fixture* fixture, VocalPriority priority)
{
    if (_showGore && (_pelvisBody || _chestBody || _headBody)) {
        int index = fmodf(floorf(fixture->GetBody()->GetPosition().x * 10000.0f), 10.0f);
        addVocalsWithName(randomVocals(index), priority);
    }
}

// @0059d840
// Explosion: smash head/chest/pelvis when the blast ratio exceeds a mass-dependent threshold.
void CharacterB2D::explodeShape(b2Fixture* fixture, float ratio)
{
    if (fixture == _headFixture) {
        if (ratio > 0.85f) {
            if (_helmetOn && !_helmetBody) {
                helmetSmash(ratio);
            } else {
                headSmash(ratio);
            }
        }
    } else if (fixture == _chestFixture) {
        float threshold = fmaxf(_chestBody->GetMass() / 0.172224f * -0.15f + 1.0f, 0.7f);
        if (threshold < ratio) {
            chestSmash(ratio);
        }
    } else if (fixture == _pelvisFixture) {
        float threshold = fmaxf(_pelvisBody->GetMass() / 0.073728f * -0.15f + 1.0f, 0.7f);
        if (threshold < ratio) {
            pelvisSmash(ratio);
        }
    }
}

// @0059d924
void CharacterB2D::setVehicle(Vehicle* vehicle)
{
    _vehicle = vehicle;
    vehicle->retain();
    _vehicle->addCharacter(this);
}

// @0059d95c
Vehicle* CharacterB2D::getVehicle()
{
    return _vehicle;
}

// @0059d964
void CharacterB2D::scaleJointLimit(b2RevoluteJoint* joint, float percent)
{
    if (_jointLimits.find(joint) != _jointLimits.end()) {
        _jointLimits[joint] *= percent;
    }
}

// @0059da78
void CharacterB2D::multiplyElbowLigamentLimit(float factor)
{
    _elbowLigamentLimit *= factor;
}

// @0059da88
void CharacterB2D::multiplyKneeLigamentLimit(float factor)
{
    _kneeLigamentLimit *= factor;
}

// @0059da98
void CharacterB2D::jointWillBeDestroyed(b2Joint* joint)
{
    if (_gripJoint1 == joint) {
        _gripJoint1 = nullptr;
        removePostSolve(_lowerArm1Fixture);
    } else if (_gripJoint2 == joint) {
        _gripJoint2 = nullptr;
        removePostSolve(_lowerArm2Fixture);
    } else if (online::flashLevel()) {
        onlineUserVehicleJointDestroyed(joint);  // ONLINE (PC addition): user-vehicle arm joints
    }
}

// @0059dad4
// Hands start reporting contacts (grabAction1/2 in handleContactResults) while attached.
void CharacterB2D::startGrab()
{
    if (!_dead && !_grabbing) {
        _grabbing = true;
        if (_shoulderJoint1 && _elbowJoint1) {
            addToPostSolve(_lowerArm1Fixture);
        }
        if (_shoulderJoint2 && _elbowJoint2) {
            addToPostSolve(_lowerArm2Fixture);
        }
    }
}

// @0059db48
void CharacterB2D::endGrab()
{
    if (_grabbing) {
        removePostSolve(_lowerArm1Fixture);
        removePostSolve(_lowerArm2Fixture);
        _grabbing = false;
        if (_gripJoint1) {
            getSession()->getDestructionListener()->removeJointListener(this, _gripJoint1);
            getWorld()->DestroyJoint(_gripJoint1);
            _lowerArm1Body->GetFixtureList()->Refilter();
            _gripJoint1 = nullptr;
        }
        if (_gripJoint2) {
            getSession()->getDestructionListener()->removeJointListener(this, _gripJoint2);
            getWorld()->DestroyJoint(_gripJoint2);
            _lowerArm2Body->GetFixtureList()->Refilter();
            _gripJoint2 = nullptr;
        }
        openHand1(true);
        openHand2(true);
    }
}

// @0059dd0c
int CharacterB2D::getFluidType()
{
    return _showGore;
}

// @0059dd14
bool CharacterB2D::getShowGore()
{
    return _showGore;
}

// @0059dd1c
// DebugLayer: alternately queues a random clip with priority 6 and 1.
void CharacterB2D::debugFunc(float a, float b, float c)
{
    static int priority = 1;  // .data @00abb5f8
    priority = (priority == 6) ? 1 : 6;
    playRandomVocals(_headFixture, (VocalPriority)priority);
}

// @0059dd3c
void CharacterB2D::debugFunction(int value)
{
    neckBreak(_spineLimit + -0.001f, true, true);
}

// ---------------------------------------------------------------------------------------------
// Wounds (1)
// ---------------------------------------------------------------------------------------------

// @0059dd5c
void CharacterB2D::addNeckWoundToChest()
{
    _neckWoundSprite = Sprite::createWithSpriteFrameName(_name + "_neck.png");
    _neckWoundSprite->setAnchorPoint(Vec2(0, 0));
    _chestSprite->addChild(_neckWoundSprite, -1);
}

// @0059dee4
void CharacterB2D::addNeckBloodFlow()
{
    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        ValueMap joints = _bodiesDict.at("joints").asValueMap();
        Vec2 anchor = PointFromString(joints.at("spineAnchor").asString());
        _neckBloodFlow = FlowEmitter::createBloodFlow(2.5f, 4.0f, 500, _chestBody,
                                                      b2Vec2(anchor.x, anchor.y), 90.0f);
        if (_neckBloodFlow) {
            _neckBloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_neckBloodFlow);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Poses
// ---------------------------------------------------------------------------------------------

// @0059e1d0
void CharacterB2D::setJoint(b2RevoluteJoint* joint, float angle, float gain, float maxSpeed)
{
    if (!joint->IsMotorEnabled()) {
        joint->EnableMotor(true);
    }
    float target = joint->GetUpperLimit() + angle;
    float difference = owb2::jointAngle(joint) - target;
    float distance = difference < 0.0f ? -difference : difference;
    float speed = b2Min(distance * distance * gain, maxSpeed);
    joint->SetMotorSpeed(difference < 0.0f ? speed : -speed);
}

// @0059e264
void CharacterB2D::setCurrentPose(CharacterPose pose)
{
    if (_currentPose != pose && (_currentPose = pose) == CharacterPoseNone) {
        cancelPose();
    }
}

// @0059e280
void CharacterB2D::cancelPose()
{
    if (_neckJoint) {
        _neckJoint->EnableMotor(false);
    }
    if (_shoulderJoint1) {
        _shoulderJoint1->EnableMotor(false);
    }
    if (_shoulderJoint2) {
        _shoulderJoint2->EnableMotor(false);
    }
    if (_elbowJoint1) {
        _elbowJoint1->EnableMotor(false);
    }
    if (_elbowJoint2) {
        _elbowJoint2->EnableMotor(false);
    }
    if (_hipJoint1) {
        _hipJoint1->EnableMotor(false);
    }
    if (_hipJoint2) {
        _hipJoint2->EnableMotor(false);
    }
    if (_kneeJoint1) {
        _kneeJoint1->EnableMotor(false);
    }
    if (_kneeJoint2) {
        _kneeJoint2->EnableMotor(false);
    }
}

// @0059e334
// Control bit 0x01 while ejected: arms forward, legs back.
void CharacterB2D::supermanPose()
{
    if (_neckJoint) {
        setJoint(_neckJoint, -0.5f, 1.0f, 20.0f);
    }
    if (_shoulderJoint1) {
        setJoint(_shoulderJoint1, 0.0f, 10.0f, 20.0f);
        if (_elbowJoint1 && !_upperArm3Body) {
            setJoint(_elbowJoint1, -2.5f, 7.5f, 20.0f);
        }
    }
    if (_shoulderJoint2) {
        setJoint(_shoulderJoint2, 0.0f, 10.0f, 20.0f);
        if (_elbowJoint2 && !_upperArm4Body) {
            setJoint(_elbowJoint2, -2.5f, 7.5f, 20.0f);
        }
    }
    if (_waistJoint) {
        if (_hipJoint1) {
            setJoint(_hipJoint1, -2.6f, 1.0f, 20.0f);
            if (_kneeJoint1 && !_upperLeg3Body) {
                setJoint(_kneeJoint1, 0.0f, 5.0f, 20.0f);
            }
        }
        if (_hipJoint2) {
            setJoint(_hipJoint2, -2.6f, 1.0f, 20.0f);
            if (_kneeJoint2 && !_upperLeg4Body) {
                setJoint(_kneeJoint2, 0.0f, 5.0f, 20.0f);
            }
        }
    }
}

// @0059e750
// Control bit 0x02 while ejected.
void CharacterB2D::tuckPose()
{
    if (_neckJoint) {
        setJoint(_neckJoint, -0.69f, 1.0f, 20.0f);
    }
    if (_shoulderJoint1) {
        setJoint(_shoulderJoint1, -3.0f, 10.0f, 20.0f);
        if (_elbowJoint1 && !_upperArm3Body) {
            setJoint(_elbowJoint1, -1.5f, 7.5f, 20.0f);
        }
    }
    if (_shoulderJoint2) {
        setJoint(_shoulderJoint2, -3.0f, 10.0f, 20.0f);
        if (_elbowJoint2 && !_upperArm4Body) {
            setJoint(_elbowJoint2, -1.5f, 7.5f, 20.0f);
        }
    }
    if (_waistJoint) {
        if (_hipJoint1) {
            setJoint(_hipJoint1, 0.0f, 1.0f, 20.0f);
            if (_kneeJoint1 && !_upperLeg3Body) {
                setJoint(_kneeJoint1, -2.0f, 5.0f, 20.0f);
            }
        }
        if (_hipJoint2) {
            setJoint(_hipJoint2, 0.0f, 1.0f, 20.0f);
            if (_kneeJoint2 && !_upperLeg4Body) {
                setJoint(_kneeJoint2, -2.0f, 5.0f, 20.0f);
            }
        }
    }
}

// @0059eb64
// Control bit 0x04 while ejected.
void CharacterB2D::archPose()
{
    if (_neckJoint) {
        setJoint(_neckJoint, -0.5f, 1.0f, 20.0f);
    }
    if (_shoulderJoint1) {
        setJoint(_shoulderJoint1, 0.0f, 20.0f, 20.0f);
        if (_elbowJoint1 && !_upperArm3Body) {
            setJoint(_elbowJoint1, -1.0f, 15.0f, 20.0f);
        }
    }
    if (_shoulderJoint2) {
        setJoint(_shoulderJoint2, 0.0f, 20.0f, 20.0f);
        if (_elbowJoint2 && !_upperArm4Body) {
            setJoint(_elbowJoint2, -1.0f, 15.0f, 20.0f);
        }
    }
    if (_waistJoint) {
        if (_hipJoint1) {
            setJoint(_hipJoint1, -3.0f, 1.0f, 20.0f);
            if (_kneeJoint1 && !_upperLeg3Body) {
                setJoint(_kneeJoint1, -2.0f, 10.0f, 20.0f);
            }
        }
        if (_hipJoint2) {
            setJoint(_hipJoint2, -3.0f, 1.0f, 20.0f);
            if (_kneeJoint2 && !_upperLeg4Body) {
                setJoint(_kneeJoint2, -2.0f, 10.0f, 20.0f);
            }
        }
    }
}

// @0059ef74
// Control bit 0x08 while ejected.
void CharacterB2D::pushupPose()
{
    if (_neckJoint) {
        setJoint(_neckJoint, -0.5f, 1.0f, 20.0f);
    }
    if (_shoulderJoint1) {
        setJoint(_shoulderJoint1, -1.7f, 20.0f, 20.0f);
        if (_elbowJoint1 && !_upperArm3Body) {
            setJoint(_elbowJoint1, -2.5f, 15.0f, 20.0f);
        }
    }
    if (_shoulderJoint2) {
        setJoint(_shoulderJoint2, -1.7f, 20.0f, 20.0f);
        if (_elbowJoint2 && !_upperArm4Body) {
            setJoint(_elbowJoint2, -2.5f, 15.0f, 20.0f);
        }
    }
    if (_waistJoint) {
        if (_hipJoint1) {
            setJoint(_hipJoint1, -2.6f, 1.0f, 20.0f);
            if (_kneeJoint1 && !_upperLeg3Body) {
                setJoint(_kneeJoint1, 0.0f, 10.0f, 20.0f);
            }
        }
        if (_hipJoint2) {
            setJoint(_hipJoint2, -2.6f, 1.0f, 20.0f);
            if (_kneeJoint2 && !_upperLeg4Body) {
                setJoint(_kneeJoint2, 0.0f, 10.0f, 20.0f);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Debug (DebugLayer buttons): break/smash with a force relative to the limit
// ---------------------------------------------------------------------------------------------

// @0059f3a4
void CharacterB2D::debugElbowBreak1(float value)
{
    if (_elbowJoint1) {
        elbowBreak1(_elbowBreakLimit + value);
    }
}

// @0059f3bc
void CharacterB2D::debugElbowBreak2(float value)
{
    if (_elbowJoint2) {
        elbowBreak2(_elbowBreakLimit + value);
    }
}

// @0059f3d4
void CharacterB2D::debugShoulderBreak1(float value)
{
    if (_shoulderJoint1) {
        shoulderBreak1(_shoulderSnapLimit + value, true);
    }
}

// @0059f3f0
void CharacterB2D::debugShoulderBreak2(float value)
{
    if (_shoulderJoint2) {
        shoulderBreak2(_shoulderSnapLimit + value, true);
    }
}

// @0059f40c
void CharacterB2D::debugHipBreak1(float value)
{
    if (_hipJoint1) {
        hipBreak1(_hipSnapLimit + value, true);
    }
}

// @0059f428
void CharacterB2D::debugHipBreak2(float value)
{
    if (_hipJoint2) {
        hipBreak2(_hipSnapLimit + value, true);
    }
}

// @0059f444
void CharacterB2D::debugKneeBreak1(float value)
{
    if (_kneeJoint1) {
        kneeBreak1(_kneeBreakLimit + value);
    }
}

// @0059f45c
void CharacterB2D::debugKneeBreak2(float value)
{
    if (_kneeJoint2) {
        kneeBreak2(_kneeBreakLimit + value);
    }
}

// @0059f474
void CharacterB2D::debugTorsoBreak(float value)
{
    if (_waistJoint) {
        torsoBreak(_torsoBreakLimit + value, true, true, false);
    }
}

// @0059f498
void CharacterB2D::debugNeckBreak(float value)
{
    if (_neckJoint) {
        neckBreak(_spineLimit + value, true, true);
    }
}

// @0059f4b8
void CharacterB2D::debugHeadSmash(float value)
{
    if (_headBody) {
        headSmash(value);
    }
}

// @0059f4c8
void CharacterB2D::debugChestSmash(float value)
{
    if (_chestBody) {
        chestSmash(value);
    }
}

// @0059f4d8
void CharacterB2D::debugPelvisSmash(float value)
{
    if (_pelvisBody) {
        pelvisSmash(value);
    }
}

// @0059f4e8
void CharacterB2D::debugFoot1Smash(float value)
{
    foot1Smash(value);
}

// @0059f4ec
void CharacterB2D::debugFoot2Smash(float value)
{
    foot2Smash(value);
}

// @0059f4f0
void CharacterB2D::debugHelmetSmash(float value)
{
    if (_helmetOn) {
        helmetSmash(value);
    }
}

// ---------------------------------------------------------------------------------------------
// Wounds and blood flows (2)
// ---------------------------------------------------------------------------------------------

// @0059f500
void CharacterB2D::addShoulderWoundToChest()
{
    _shoulderWoundSprite = Sprite::createWithSpriteFrameName(_name + "_shoulder_wound.png");
    _shoulderWoundSprite->setAnchorPoint(Vec2(0, 0));
    _chestSprite->addChild(_shoulderWoundSprite);
}

// @0059f68c
// Dislocated (force up to the snap limit): from the loose half of the arm; torn off: from the
// shoulder socket on the chest.
void CharacterB2D::addShoulder1BloodFlow(float force)
{
    EmitterNode* particles = getSession()->getParticlesMidground();
    if (particles) {
        if (force <= _shoulderSnapLimit) {
            if (!_upperArm3Body) {
                return;
            }
            _shoulder1BloodFlow =
                FlowEmitter::createBloodFlow(1.0f, 3.0f, 200, _upperArm3Body, b2Vec2_zero, -90.0f);
        } else {
            _shoulder1BloodFlow = FlowEmitter::createBloodFlow(0.0f, 1.0f, 500, _chestBody,
                                                               _shoulderBloodFlowPos, -90.0f);
        }
        if (_shoulder1BloodFlow) {
            _shoulder1BloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_shoulder1BloodFlow);
        }
    }
}

// @0059f750
void CharacterB2D::addArm1BloodFlow()
{
    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        b2PolygonShape* shape = (b2PolygonShape*)_upperArm1Body->GetFixtureList()->GetShape();
        _arm1BloodFlow = FlowEmitter::createBloodFlow(
            1.0f, 3.0f, 500, _upperArm1Body, b2Vec2(0.0f, shape->m_vertices[2].y * 0.8f), 90.0f);
        if (_arm1BloodFlow) {
            _arm1BloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_arm1BloodFlow);
        }
    }
}

// @0059f7e4
void CharacterB2D::addShoulder2BloodFlow(float force)
{
    EmitterNode* particles = getSession()->getParticlesBackground();
    if (particles) {
        if (force <= _shoulderSnapLimit) {
            if (!_upperArm4Body) {
                return;
            }
            _shoulder2BloodFlow =
                FlowEmitter::createBloodFlow(1.0f, 3.0f, 200, _upperArm4Body, b2Vec2_zero, -90.0f);
        } else {
            _shoulder2BloodFlow = FlowEmitter::createBloodFlow(0.0f, 1.0f, 500, _chestBody,
                                                               _shoulderBloodFlowPos, -90.0f);
        }
        if (_shoulder2BloodFlow) {
            _shoulder2BloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_shoulder2BloodFlow);
        }
    }
}

// @0059f8a8
void CharacterB2D::addArm2BloodFlow()
{
    EmitterNode* particles = getSession()->getParticlesBackground();
    if (particles) {
        b2PolygonShape* shape = (b2PolygonShape*)_upperArm2Body->GetFixtureList()->GetShape();
        _arm2BloodFlow = FlowEmitter::createBloodFlow(
            1.0f, 3.0f, 500, _upperArm2Body, b2Vec2(0.0f, shape->m_vertices[2].y * 0.8f), 90.0f);
        if (_arm2BloodFlow) {
            _arm2BloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_arm2BloodFlow);
        }
    }
}

// @0059f93c
void CharacterB2D::addHip1BloodFlow(float force)
{
    EmitterNode* particles = getSession()->getParticlesMidground();
    if (particles) {
        if (force <= _hipSnapLimit) {
            b2PolygonShape* shape = (b2PolygonShape*)_upperLeg3Body->GetFixtureList()->GetShape();
            _hip1BloodFlow = FlowEmitter::createBloodFlow(
                1.0f, 3.0f, 200, _upperLeg3Body, b2Vec2(0.0f, shape->m_vertices[0].y), -90.0f);
        } else {
            _hip1BloodFlow = FlowEmitter::createBloodFlow(0.0f, 1.0f, 500, _pelvisBody,
                                                          _pelvisBloodFlowPos, -90.0f);
        }
        if (_hip1BloodFlow) {
            _hip1BloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_hip1BloodFlow);
        }
    }
}

// @0059fa00
// From the end of thigh 1 when it was torn off, from its origin when it only dislocated.
void CharacterB2D::addThigh1BloodFlow(float force)
{
    EmitterNode* particles = getSession()->getParticlesBackground();
    if (particles) {
        b2Vec2 offset = b2Vec2_zero;
        if (_hipSnapLimit < force) {
            offset.y = ((b2PolygonShape*)_upperLeg1Body->GetFixtureList()->GetShape())->m_vertices[2].y;
        }
        _thigh1BloodFlow =
            FlowEmitter::createBloodFlow(1.0f, 3.0f, 200, _upperLeg1Body, offset, 90.0f);
        if (_thigh1BloodFlow) {
            _thigh1BloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_thigh1BloodFlow);
        }
    }
}

// @0059faac
void CharacterB2D::addHip2BloodFlow(float force)
{
    EmitterNode* particles = getSession()->getParticlesBackground();
    if (particles) {
        if (force <= _hipSnapLimit) {
            b2PolygonShape* shape = (b2PolygonShape*)_upperLeg4Body->GetFixtureList()->GetShape();
            _hip2BloodFlow = FlowEmitter::createBloodFlow(
                1.0f, 3.0f, 200, _upperLeg4Body, b2Vec2(0.0f, shape->m_vertices[0].y), -90.0f);
        } else {
            _hip2BloodFlow = FlowEmitter::createBloodFlow(0.0f, 1.0f, 500, _pelvisBody,
                                                          _pelvisBloodFlowPos, -90.0f);
        }
        if (_hip2BloodFlow) {
            _hip2BloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_hip2BloodFlow);
        }
    }
}

// @0059fb70
void CharacterB2D::addThigh2BloodFlow(float force)
{
    EmitterNode* particles = getSession()->getParticlesBackground();
    if (particles) {
        b2Vec2 offset = b2Vec2_zero;
        if (_hipSnapLimit < force) {
            offset.y = ((b2PolygonShape*)_upperLeg2Body->GetFixtureList()->GetShape())->m_vertices[2].y;
        }
        _thigh2BloodFlow =
            FlowEmitter::createBloodFlow(1.0f, 3.0f, 200, _upperLeg2Body, offset, 90.0f);
        if (_thigh2BloodFlow) {
            _thigh2BloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_thigh2BloodFlow);
        }
    }
}

// @0059fc1c
void CharacterB2D::addPelvisWoundToPelvis()
{
    Sprite* wound = Sprite::createWithSpriteFrameName(_name + "_pelvisWound.png");
    wound->setAnchorPoint(Vec2(0, 0));
    _pelvisSprite->addChild(wound);
}

// @0059fd9c
void CharacterB2D::addStomchBloodFlow()
{
    EmitterNode* particles = getSession()->getParticlesBackground();
    if (particles && _chestBody) {
        b2PolygonShape* shape = (b2PolygonShape*)_chestBody->GetFixtureList()->GetShape();
        _stomachBloodFlow = FlowEmitter::createBloodFlow(
            2.0f, 3.0f, 500, _chestBody, b2Vec2(0.0f, shape->m_vertices[0].y * 0.5f), -90.0f);
        if (_stomachBloodFlow) {
            _stomachBloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_stomachBloodFlow);
        }
    }
}

// @0059fe2c
// The head sprite is replaced by the one with the neck stump (moved to the character
// foreground, same z-order, position and rotation); the helmet is re-attached if still on.
void CharacterB2D::addNeckWoundToHead()
{
    int zOrder = _headSprite->getLocalZOrder();
    Vec2 position = _headSprite->getPosition();
    float rotation = _headSprite->getRotation();
    _headSprite->removeFromParentAndCleanup(false);
    _headSprite = Sprite::createWithSpriteFrameName(_name + "_head_2.png");
    _headSprite->setPosition(position);
    _headSprite->setRotation(rotation);
    getSession()->getCharacterForeground()->addChild(_headSprite, zOrder);
    _headBody->SetUserData(_headSprite);
    if (_helmetOn && !_helmetBody) {
        Sprite* helmet = createHelmetSprite();
        helmet->setAnchorPoint(Vec2(0, 0));
        _headSprite->addChild(helmet);
    }
}

// @005a0088
void CharacterB2D::addHeadBloodFlow()
{
    EmitterNode* particles = getSession()->getParticlesMidground();
    if (particles && _headBody) {
        b2Shape* shape = _headBody->GetFixtureList()->GetShape();
        _headBloodFlow = FlowEmitter::createBloodFlow(
            2.5f, 4.0f, 150, _headBody, b2Vec2(0.0f, shape->m_radius * -0.5f), -90.0f);
        if (_headBloodFlow) {
            _headBloodFlow->setEmitterDelegate(this);
            particles->addChildEmitter(_headBloodFlow);
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Voice
// ---------------------------------------------------------------------------------------------

// @005a0118
void CharacterB2D::voiceSoundFinishedPlaying()
{
    _currentVocalPriority = VocalPriorityNone;
    _voiceSound = nullptr;
}

// @005a0124
std::string CharacterB2D::randomVocals(int index)
{
    return std::string(_randomVocals[index]);
}

// @005a01b4
// The smash names are preloaded in lower case, unlike the names played (kept as in the binary).
void CharacterB2D::preloadSounds()
{
    SoundController* soundController = Settings::getInstance()->getSoundController();
    soundController->preloadSound("pelvisSmash");
    soundController->preloadSound("NeckBreak");
    soundController->preloadSound("LimbRip3");
    soundController->preloadSound("LimbRip4");
    soundController->preloadSound("LimbRip2");
    soundController->preloadSound("LigTear2");
    soundController->preloadSound("LimbRip1");
    soundController->preloadSound("LigTear1");
    soundController->preloadSound("ImpaleSpikes3");
    soundController->preloadSound("ImpaleSpikes2");
    soundController->preloadSound("ImpaleSpikes1");
    soundController->preloadSound("chestSmash");
    soundController->preloadSound("headSmash");
    soundController->preloadSound("BoneBreak4");
    soundController->preloadSound("BoneBreak2");
    soundController->preloadSound("BoneBreak3");
    soundController->preloadSound("BoneBreak1");
}

// ---------------------------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------------------------

// @005a079c
b2Body* CharacterB2D::getCentralBody()
{
    return _heartBody ? _heartBody : _chestBody;
}

// @005a07b0
b2Body* CharacterB2D::getHeadBody()
{
    return _headBody;
}

// @005a07b8
b2Body* CharacterB2D::getChestBody()
{
    return _chestBody;
}

// @005a07c0
b2Body* CharacterB2D::getUpperArm1Body()
{
    return _upperArm1Body;
}

// @005a07c8
b2Body* CharacterB2D::getUpperArm2Body()
{
    return _upperArm2Body;
}

// @005a07d0
b2Body* CharacterB2D::getUpperArm3Body()
{
    return _upperArm3Body;
}

// @005a07d8
b2Body* CharacterB2D::getUpperArm4Body()
{
    return _upperArm4Body;
}

// @005a07e0
b2Body* CharacterB2D::getLowerArm1Body()
{
    return _lowerArm1Body;
}

// @005a07e8
b2Body* CharacterB2D::getLowerArm2Body()
{
    return _lowerArm2Body;
}

// @005a07f0
b2Body* CharacterB2D::getPelvisBody()
{
    return _pelvisBody;
}

// @005a07f8
b2Body* CharacterB2D::getUpperLeg1Body()
{
    return _upperLeg1Body;
}

// @005a0800
b2Body* CharacterB2D::getUpperLeg2Body()
{
    return _upperLeg2Body;
}

// @005a0808
b2Body* CharacterB2D::getUpperLeg3Body()
{
    return _upperLeg3Body;
}

// @005a0810
b2Body* CharacterB2D::getUpperLeg4Body()
{
    return _upperLeg4Body;
}

// @005a0818
b2Body* CharacterB2D::getLowerLeg1Body()
{
    return _lowerLeg1Body;
}

// @005a0820
b2Body* CharacterB2D::getLowerLeg2Body()
{
    return _lowerLeg2Body;
}

// @005a0828
b2RevoluteJoint* CharacterB2D::getNeckJoint()
{
    return _neckJoint;
}

// @005a0830
b2RevoluteJoint* CharacterB2D::getWaistJoint()
{
    return _waistJoint;
}

// @005a0838
b2RevoluteJoint* CharacterB2D::getShoulderJoint1()
{
    return _shoulderJoint1;
}

// @005a0840
b2RevoluteJoint* CharacterB2D::getShoulderJoint2()
{
    return _shoulderJoint2;
}

// @005a0848
b2RevoluteJoint* CharacterB2D::getElbowJoint1()
{
    return _elbowJoint1;
}

// @005a0850
b2RevoluteJoint* CharacterB2D::getElbowJoint2()
{
    return _elbowJoint2;
}

// @005a0858
b2RevoluteJoint* CharacterB2D::getHipJoint1()
{
    return _hipJoint1;
}

// @005a0860
b2RevoluteJoint* CharacterB2D::getHipJoint2()
{
    return _hipJoint2;
}

// @005a0868
b2RevoluteJoint* CharacterB2D::getKneeJoint1()
{
    return _kneeJoint1;
}

// @005a0870
b2RevoluteJoint* CharacterB2D::getKneeJoint2()
{
    return _kneeJoint2;
}

// @005a0878 (EmitterDelegate thunk @005a0958 is compiler-generated)
void CharacterB2D::emitterComplete(Emitter* emitter)
{
    if (_neckBloodFlow == emitter) {
        _neckBloodFlow = nullptr;
    } else if (_headBloodFlow == emitter) {
        _headBloodFlow = nullptr;
    } else if (_shoulder1BloodFlow == emitter) {
        _shoulder1BloodFlow = nullptr;
    } else if (_shoulder2BloodFlow == emitter) {
        _shoulder2BloodFlow = nullptr;
    } else if (_stomachBloodFlow == emitter) {
        _stomachBloodFlow = nullptr;
    } else if (_hip1BloodFlow == emitter) {
        _hip1BloodFlow = nullptr;
    } else if (_hip2BloodFlow == emitter) {
        _hip2BloodFlow = nullptr;
    } else if (_arm1BloodFlow == emitter) {
        _arm1BloodFlow = nullptr;
    } else if (_arm2BloodFlow == emitter) {
        _arm2BloodFlow = nullptr;
    } else if (_thigh1BloodFlow == emitter) {
        _thigh1BloodFlow = nullptr;
    } else if (_thigh2BloodFlow == emitter) {
        _thigh2BloodFlow = nullptr;
    }
}

// @005a0a38
void CharacterB2D::setMourner(CharacterB2D* mourner)
{
    _mourner = mourner;
}

// ---------------------------------------------------------------------------------------------
// RESTORED (PC addition): the browser game's lawnmower blade (Flash CharacterB2D.grindShape /
// removeBody). Called only by src/restored/LawnMower; the mobile game never reaches these.
// ---------------------------------------------------------------------------------------------

bool CharacterB2D::ownsBody(b2Body* body)
{
    if (body == nullptr) {
        return false;
    }
    b2Body* bodies[] = {_headBody,      _chestBody,     _upperArm1Body, _upperArm2Body,
                        _lowerArm1Body, _lowerArm2Body, _pelvisBody,    _upperLeg1Body,
                        _upperLeg2Body, _lowerLeg1Body, _lowerLeg2Body, _upperArm3Body,
                        _upperArm4Body, _upperLeg3Body, _upperLeg4Body, _heartBody,
                        _brainBody,     _helmetBody};
    for (b2Body* b : bodies) {
        if (b == body) {
            return true;
        }
    }
    return false;
}

void CharacterB2D::grindFixture(b2Fixture* fixture)
{
    auto stop = [](Emitter* e) {
        if (e) {
            e->stop();
        }
    };
    if (fixture == _headFixture || fixture == _chestFixture || fixture == _pelvisFixture) {
        removePostSolve(fixture);
        _contactImpulseDict.erase(fixture);
        removeFromContactResultBufferDict(fixture);
    }
    if (fixture == _headFixture) {
        stop(_headBloodFlow);
    } else if (fixture == _chestFixture) {
        stop(_neckBloodFlow);
        stop(_shoulder1BloodFlow);
        stop(_shoulder2BloodFlow);
        stop(_stomachBloodFlow);
    } else if (fixture == _pelvisFixture) {
        stop(_hip1BloodFlow);
        stop(_hip2BloodFlow);
    } else if (_upperArm1Body && fixture == _upperArm1Body->GetFixtureList()) {
        stop(_arm1BloodFlow);
    } else if (_upperArm2Body && fixture == _upperArm2Body->GetFixtureList()) {
        stop(_arm2BloodFlow);
    } else if (_upperLeg1Body && fixture == _upperLeg1Body->GetFixtureList()) {
        stop(_thigh1BloodFlow);
    } else if (_upperLeg2Body && fixture == _upperLeg2Body->GetFixtureList()) {
        stop(_thigh2BloodFlow);
    }
}

void CharacterB2D::grindBody(b2Body* body)
{
    // Flash tests joint.broken; here a joint is intact while its pointer is set and (for the
    // shoulders/hips) no dislocated stub body exists. The neck breaks at the spine limit (the
    // Flash force 0 would make no sense to SpinalCord's vertebra count).
    bool shoulder1 = _shoulderJoint1 && !_upperArm3Body;
    bool shoulder2 = _shoulderJoint2 && !_upperArm4Body;
    bool hip1 = _hipJoint1 && !_upperLeg3Body;
    bool hip2 = _hipJoint2 && !_upperLeg4Body;
    if (body == _headBody) {
        if (_neckJoint) {
            neckBreak(_spineLimit, true, true);
        }
        if (_spinalCord) {
            _spinalCord->spineBreak2();
        }
    } else if (body == _chestBody) {
        if (_neckJoint) {
            neckBreak(_spineLimit, true, true);
        }
        if (shoulder1) {
            shoulderBreak1(0.0f, true);
        }
        if (shoulder2) {
            shoulderBreak2(0.0f, true);
        }
        if (_waistJoint) {
            torsoBreak(0.0f, true, true, false);
        }
        if (_intestineChain) {
            _intestineChain->intestineBreak2();
        }
        if (_spinalCord) {
            _spinalCord->spineBreak1();
        }
    } else if (body == _pelvisBody) {
        if (hip1) {
            hipBreak1(0.0f, true);
        }
        if (hip2) {
            hipBreak2(0.0f, true);
        }
        if (_waistJoint) {
            torsoBreak(0.0f, true, true, false);
        }
        if (_intestineChain) {
            _intestineChain->intestineBreak1();
        }
    } else if (body == _upperArm1Body) {
        if (shoulder1) {
            shoulderBreak1(0.0f, true);
        }
        if (_elbowJoint1) {
            elbowBreak1(1000.0f);
        }
        if (_arm1BloodFlow) {
            _arm1BloodFlow->stop();
        }
    } else if (body == _upperArm2Body) {
        if (shoulder2) {
            shoulderBreak2(0.0f, true);
        }
        if (_elbowJoint2) {
            elbowBreak2(1000.0f);
        }
        if (_arm2BloodFlow) {
            _arm2BloodFlow->stop();
        }
    } else if (body == _upperLeg1Body) {
        if (hip1) {
            hipBreak1(0.0f, true);
        }
        if (_kneeJoint1) {
            kneeBreak1(1000.0f);
        }
        if (_thigh1BloodFlow) {
            _thigh1BloodFlow->stop();
        }
    } else if (body == _upperLeg2Body) {
        if (hip2) {
            hipBreak2(0.0f, true);
        }
        if (_kneeJoint2) {
            kneeBreak2(1000.0f);
        }
        if (_thigh2BloodFlow) {
            _thigh2BloodFlow->stop();
        }
    }
}

// ---------------------------------------------------------------------------------------------
// QOL (PC addition): re-grab vehicle (qol::regrabVehicle, Vehicle::qolTryRemount). Not in the
// original, where a grabbed vehicle is held like anything else.
// ---------------------------------------------------------------------------------------------

namespace {
// Grabs during the first half second after the ejection do not count (summed physics steps).
const float kQolRemountDelay = 0.5f;
}  // namespace

bool CharacterB2D::qolHasInjury(CharacterInjury injury) const
{
    return std::find(_qolInjuries.begin(), _qolInjuries.end(), injury) != _qolInjuries.end();
}

bool CharacterB2D::qolLostLowerArm(int arm) const
{
    return arm == 1 ? qolHasInjury(CharacterInjuryShoulder1Break) || qolHasInjury(CharacterInjuryElbow1Break)
                    : qolHasInjury(CharacterInjuryShoulder2Break) || qolHasInjury(CharacterInjuryElbow2Break);
}

bool CharacterB2D::qolLostUpperLeg(int leg) const
{
    return qolHasInjury(leg == 1 ? CharacterInjuryHip1Break : CharacterInjuryHip2Break);
}

bool CharacterB2D::qolLostLowerLeg(int leg, bool footSmashCounts) const
{
    if (leg == 1) {
        return qolHasInjury(CharacterInjuryHip1Break) || qolHasInjury(CharacterInjuryKnee1Break) ||
               (footSmashCounts && qolHasInjury(CharacterInjuryFoot1Smash));
    }
    return qolHasInjury(CharacterInjuryHip2Break) || qolHasInjury(CharacterInjuryKnee2Break) ||
           (footSmashCounts && qolHasInjury(CharacterInjuryFoot2Smash));
}

bool CharacterB2D::qolFit()
{
    return !_dead && !_dying && _neckJoint && _waistJoint && _headBody && _chestBody && _pelvisBody;
}

std::vector<b2Body*> CharacterB2D::qolParts()
{
    return {_headBody,      _chestBody,     _upperArm1Body, _upperArm2Body,
            _lowerArm1Body, _lowerArm2Body, _pelvisBody,    _upperLeg1Body,
            _upperLeg2Body, _lowerLeg1Body, _lowerLeg2Body};
}

std::vector<b2Fixture*> CharacterB2D::qolLostFixtures()
{
    std::vector<b2Body*> bodies;
    if (qolHasInjury(CharacterInjuryShoulder1Break)) {
        bodies.push_back(_upperArm1Body);
    }
    if (qolHasInjury(CharacterInjuryShoulder2Break)) {
        bodies.push_back(_upperArm2Body);
    }
    if (qolHasInjury(CharacterInjuryShoulder1Break) || qolHasInjury(CharacterInjuryElbow1Break)) {
        bodies.push_back(_lowerArm1Body);
    }
    if (qolHasInjury(CharacterInjuryShoulder2Break) || qolHasInjury(CharacterInjuryElbow2Break)) {
        bodies.push_back(_lowerArm2Body);
    }
    if (qolHasInjury(CharacterInjuryHip1Break)) {
        bodies.push_back(_upperLeg1Body);
    }
    if (qolHasInjury(CharacterInjuryHip2Break)) {
        bodies.push_back(_upperLeg2Body);
    }
    if (qolHasInjury(CharacterInjuryHip1Break) || qolHasInjury(CharacterInjuryKnee1Break)) {
        bodies.push_back(_lowerLeg1Body);
    }
    if (qolHasInjury(CharacterInjuryHip2Break) || qolHasInjury(CharacterInjuryKnee2Break)) {
        bodies.push_back(_lowerLeg2Body);
    }
    std::vector<b2Fixture*> fixtures;
    for (b2Body* body : bodies) {
        if (body && body->GetFixtureList()) {
            fixtures.push_back(body->GetFixtureList());
        }
    }
    return fixtures;
}

// The hand is still on his arm (no torn shoulder or dislocated stub, no broken elbow).
bool CharacterB2D::qolHandFree(int hand) const
{
    if (hand == 1) {
        return _shoulderJoint1 && !_upperArm3Body && _elbowJoint1;
    }
    return _shoulderJoint2 && !_upperArm4Body && _elbowJoint2;
}

bool CharacterB2D::qolGrabRemount(int hand, b2Body* other)
{
    if (!_mainCharacter || !_ejected || !_vehicle || !other || _qolEjectedTime < kQolRemountDelay ||
        !qolHandFree(hand) || !qol::regrabVehicle()) {
        return false;
    }
    // A browser user vehicle he rides now keeps him.
    if (online::flashLevel()) {
        online::UserVehicleRider* rider = online::userVehicleRider(this, false);
        if (rider && rider->vehicle) {
            return false;
        }
    }
    if (!_vehicle->qolOwnsBody(other, this) || !_vehicle->qolTryRemount(this)) {
        return false;
    }
    // Riding again: the other hand's contact of this step is no grab any more.
    _contactResultBufferDict[_lowerArm1Fixture].impulse = 0.0f;
    _contactResultBufferDict[_lowerArm2Fixture].impulse = 0.0f;
    return true;
}

// Per step while ejected: an empty grabbing hand that overlaps his vehicle (also the vehicle parts
// that do not collide with him, e.g. the lawnmower's) re-mounts him. A hand already holding
// something made its grab earlier (maybe within the first half second) and does not count.
void CharacterB2D::qolCheckRemount()
{
    if (!_grabbing || !_mainCharacter || !_vehicle || _dead || _qolEjectedTime < kQolRemountDelay ||
        !qol::regrabVehicle()) {
        return;
    }
    for (int hand = 1; hand <= 2 && _ejected; hand++) {
        if (!qolHandFree(hand) || (hand == 1 ? _gripJoint1 : _gripJoint2)) {
            continue;
        }
        b2Body* touched = _vehicle->qolTouchedBody(hand == 1 ? _lowerArm1Fixture : _lowerArm2Fixture, this);
        if (touched && qolGrabRemount(hand, touched)) {
            return;
        }
    }
}

void CharacterB2D::qolBeginRemount()
{
    endGrab();
    setCurrentPose(CharacterPoseNone);
}

void CharacterB2D::qolSetRiding()
{
    _ejected = false;
    _qolEjectedTime = 0.0f;
}

void CharacterB2D::qolRemounted()
{
    openHand1(false);
    openHand2(false);
    if (_mainCharacter) {
        if (GameplayControls* controls = getSession()->getControls()) {
            // The ejected layout keeps the boost meter; the driving layout makes a new one.
            controls->removeMeterBar();
            controls->addControls((ControlsType)Settings::getInstance()->getSelectedCharacterControlType());
        }
        Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("characterRemounted");
    }
}
