#include "online/FlashRuntime.h"  // ONLINE (PC addition)
#include "Harpoon.h"

#include <algorithm>
#include <cmath>
#include <new>
#include <string>

#include "BurstEmitter.h"
#include "DestructionListener.h"
#include "EmitterNode.h"
#include "Globals.h"
#include "HarpoonGun.h"
#include "LevelB2D.h"
#include "Patch.h"
#include "Session.h"
#include "Sound.h"

USING_NS_CC;

// Inline in the original (only visible inlined into create @005bf4e0).
Harpoon::Harpoon()
    : _mc(nullptr),
      _blood(nullptr),
      _bloodCount(0),
      _bloodComplete(false),
      _harpoonGun(nullptr),
      _harpoonBody(nullptr),
      _sensorShape(nullptr),
      _previousBody(nullptr),
      _stabbableMaterials(0),
      _fleshSound(nullptr),
      _solidSound(nullptr)
{
}

// @005c0abc (D2), @005c0b2c (D0)
Harpoon::~Harpoon()
{
    _mc = nullptr;
    _blood = nullptr;
    _fleshSound = nullptr;
    _solidSound = nullptr;
    _harpoonBody = nullptr;
    _harpoonGun = nullptr;
    _previousBody = nullptr;
    _sensorShape = nullptr;
}

// @005bf4e0
Harpoon* Harpoon::create(b2Vec2 position, float angle, b2Vec2 velocity, int zOrder)
{
    Harpoon* harpoon = new (std::nothrow) Harpoon();
    if (harpoon) {
        harpoon->init(position, angle, velocity, zOrder);
        harpoon->autorelease();
    }
    return harpoon;
}

// @005bf5cc
bool Harpoon::init(b2Vec2 position, float angle, b2Vec2 velocity, int zOrder)
{
    Node* levelItemsNode = getLevelItemsNode();
    _stabbableMaterials = 2;
    if (online::flashLevel()) _stabbableMaterials |= 4;  // ONLINE (PC addition): Flash stabs materials & 6 (food)
    _harpoonGun = nullptr;
    _bloodComplete = false;
    _bloodCount = 0;

    _mc = Sprite::createWithSpriteFrameName("harpoon.png");
    _mc->setPosition(Vec2(position.x * getPtm(), position.y * getPtm()));
    _mc->setRotation(CC_RADIANS_TO_DEGREES(angle));
    levelItemsNode->addChild(_mc, zOrder - 1);

    createBody(position, angle, velocity);
    _harpoonBody->SetUserData(_mc);

    addToBeginContact(_sensorShape);
    addToEndContact(_sensorShape);
    getLevel()->addToActions(this);
    getLevel()->addToPaintBody(_harpoonBody);
    return true;
}

// @005bf7b0
void Harpoon::createBody(b2Vec2 position, float angle, b2Vec2 velocity)
{
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.angle = angle;
    _harpoonBody = getWorld()->CreateBody(&bodyDef);

    const float ptmRatio = globals::flash::ptmRatio;
    float halfLength = 26.25f / ptmRatio;

    // Front half: stabbing sensor.
    b2PolygonShape shape;
    shape.SetAsBox(halfLength, 2.5f / ptmRatio, b2Vec2(halfLength, 0.0f), 0.0f);
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.density = 3.0f;
    fixtureDef.isSensor = true;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = -21;
    _sensorShape = _harpoonBody->CreateFixture(&fixtureDef);

    // Back half: solid (same filter group).
    shape.SetAsBox(halfLength, 2.5f / ptmRatio, b2Vec2(-26.25f / ptmRatio, 0.0f), 0.0f);
    fixtureDef.isSensor = false;
    _harpoonBody->CreateFixture(&fixtureDef);

    // Move the body origin forward to the tip region.
    _harpoonBody->SetTransform(_harpoonBody->GetWorldPoint(b2Vec2(52.5f / ptmRatio, 0.0f)), angle);
    _harpoonBody->ResetMassData();
    _harpoonBody->SetLinearVelocity(velocity);
}

// @005bf9cc
void Harpoon::setHarpoonGun(HarpoonGun* harpoonGun)
{
    _harpoonGun = harpoonGun;
}

// @005bf9d4
b2Body* Harpoon::getHarpoonBody()
{
    return _harpoonBody;
}

// @005bf9dc
void Harpoon::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (otherFixture->IsSensor()) {
        return;
    }
    b2Body* body = otherFixture->GetBody();
    b2BodyType type = body->GetType();
    int material = getLevel()->getFixtureMaterial(otherFixture);
    if (type != b2_staticBody && (material & _stabbableMaterials) == 0) {
        return;
    }
    if (std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body) != _bodiesToAdd.end()) {
        return;
    }
    if ((material & _stabbableMaterials) != 0 && otherFixture->GetUserData() != nullptr &&
        static_cast<LevelItem*>(otherFixture->GetUserData())->getFluidType() == 1) {
        addBlood();
    }
    getSession()->getDestructionListener()->addFixtureListener(otherFixture, this);
    _bodiesToAdd.push_back(body);
}

// @005bfbe0
void Harpoon::addBlood()
{
    if (_bloodCount == 3 || _bloodCount == 0) {
        float frameBase;
        if (_blood) {
            _blood->removeFromParentAndCleanup(false);
            frameBase = 3.0f;
        } else {
            frameBase = 0.0f;
        }
        float random = CCRANDOM_0_1();
        unsigned int frame = frameBase + ceilf(random * 3);
        _blood = Sprite::createWithSpriteFrameName("harpoon_blood_" + patch::to_string(frame) + ".png");
        _blood->setAnchorPoint(Vec2(1.0f, 0.5f));
        if (random < 0.5f) {
            _blood->setScaleY(-1.0f);
        }
        _blood->setPosition(
            Vec2(_mc->getTextureRect().size.width, _mc->getTextureRect().size.height * 0.5f));
        _mc->addChild(_blood);
    }
    _bloodCount++;
}

// @005bfe38
void Harpoon::fixtureWillBeDestroyed(b2Fixture* fixture)
{
    b2Body* body = fixture->GetBody();
    _bjDictionary.erase(body);

    auto it = std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body);
    if (it != _bodiesToAdd.end()) {
        _bodiesToAdd.erase(it);
    }
    it = std::find(_bodiesToRemove.begin(), _bodiesToRemove.end(), body);
    if (it != _bodiesToRemove.end()) {
        _bodiesToRemove.erase(it);
    }
}

// @005bff88
void Harpoon::jointWillBeDestroyed(b2Joint* joint)
{
    b2Body* body = joint->GetBodyB();
    _bjDictionary.erase(body);

    auto it = std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body);
    if (it != _bodiesToAdd.end()) {
        _bodiesToAdd.erase(it);
    }
    it = std::find(_bodiesToRemove.begin(), _bodiesToRemove.end(), body);
    if (it != _bodiesToRemove.end()) {
        _bodiesToRemove.erase(it);
    }
}

// @005c00d8
void Harpoon::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (otherFixture->IsSensor()) {
        return;
    }
    b2Body* body = otherFixture->GetBody();
    int material = getLevel()->getFixtureMaterial(otherFixture);
    if ((material & _stabbableMaterials) == 0 && body->GetType() != b2_staticBody) {
        return;
    }
    if (_bjDictionary.find(body) == _bjDictionary.end()) {
        if (std::find(_bodiesToAdd.begin(), _bodiesToAdd.end(), body) == _bodiesToAdd.end()) {
            return;
        }
    }
    if (std::find(_bodiesToRemove.begin(), _bodiesToRemove.end(), body) != _bodiesToRemove.end()) {
        return;
    }
    _bodiesToRemove.push_back(body);
}

// @005c0318
void Harpoon::actions()
{
    for (unsigned int i = 0; i < _bodiesToAdd.size(); i++) {
        createPrisJoint(_bodiesToAdd[i]);
    }
    _bodiesToAdd.clear();
    for (unsigned int i = 0; i < _bodiesToRemove.size(); i++) {
        removeJoint(_bodiesToRemove[i]);
    }
    _bodiesToRemove.clear();
}

// @005c039c
void Harpoon::createPrisJoint(b2Body* body)
{
    b2Fixture* fixture = body->GetFixtureList();

    b2PrismaticJointDef jointDef;
    b2Vec2 anchor = _harpoonBody->GetWorldPoint(b2Vec2(26.25f / globals::flash::ptmRatio, 0.0f));
    float angle = _harpoonBody->GetAngle();
    b2Vec2 axis(cosf(angle), sinf(angle));
    jointDef.Initialize(_harpoonBody, body, anchor, axis);
    jointDef.lowerTranslation = 0.0f;
    jointDef.upperTranslation = 0.0f;
    jointDef.enableLimit = true;
    jointDef.collideConnected = true;
    jointDef.enableMotor = true;
    jointDef.maxMotorForce = 100000.0f;
    jointDef.motorSpeed = 0.0f;
    jointDef.userData = this;
    b2Joint* joint = getWorld()->CreateJoint(&jointDef);
    getSession()->getDestructionListener()->addJointListener(joint, this);
    _bjDictionary[body] = static_cast<b2PrismaticJoint*>(joint);

    if (_harpoonGun) {
        _harpoonGun->harpoonHit(this);
    }

    getSession()->getDestructionListener()->removeFixtureListener(this, fixture);
    int material = getLevel()->getFixtureMaterial(fixture);
    if (material & _stabbableMaterials) {
        LevelItem* item = static_cast<LevelItem*>(fixture->GetUserData());
        if (!item || item->shapeImpale(fixture, true, b2Vec2(INFINITY, INFINITY), 0.0f) != 1) {
            return;
        }
        EmitterNode* particles = getSession()->getParticlesForeground();
        if (particles) {
            float ptm = getPtm();
            Emitter* blood =
                BurstEmitter::createBloodBurst(5.0f, 15.0f, Vec2(ptm * anchor.x, ptm * anchor.y), 50);
            if (blood) {
                particles->addChild(blood);
            }
        }
        if (_fleshSound) {
            return;
        }
        unsigned int variant = ceilf(CCRANDOM_0_1() * 2);
        _fleshSound = createBodySound("HarpoonFlesh" + patch::to_string(variant), _harpoonBody, 1.0f, false);
        if (_fleshSound) {
            _fleshSound->setFinishCallback([this](int&) { fleshSoundStopped(); });
        }
    } else {
        if (_solidSound) {
            return;
        }
        int variant = ceilf(CCRANDOM_0_1() * 2);
        _solidSound = createBodySound("HarpoonSolid" + patch::to_string(variant), _harpoonBody, 1.0f, false);
        if (_solidSound) {
            _solidSound->setFinishCallback([this](int&) { solidSoundStopped(); });
        }
    }
}

// @005c08d0
void Harpoon::removeJoint(b2Body* body)
{
    if (_bjDictionary.find(body) != _bjDictionary.end()) {
        b2PrismaticJoint* joint = _bjDictionary[body];
        if (body->GetType() == b2_dynamicBody) {
            getSession()->getDestructionListener()->removeJointListener(this, joint);
        }
        getWorld()->DestroyJoint(joint);
        _bjDictionary.erase(body);
    }
}

// @005c0aac
void Harpoon::fleshSoundStopped()
{
    _fleshSound = nullptr;
}

// @005c0ab4
void Harpoon::solidSoundStopped()
{
    _solidSound = nullptr;
}
