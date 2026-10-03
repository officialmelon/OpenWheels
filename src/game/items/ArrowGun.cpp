#include "ArrowGun.h"

#include <cmath>
#include <string>

#include "Arrow.h"
#include "ContactListener.h"
#include "Globals.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"
#include "Session.h"
#include "TargetRaycast.h"

USING_NS_CC;

// @005801fc
ArrowGun::ArrowGun()
    : _mc(nullptr),
      _turret(nullptr),
      _stringMC(nullptr),
      _stringMCFrame(0),
      _shouldUpdateFrame(false),
      _dontShootPlayer(false),
      _rangeSensor(nullptr),
      _targetSensor(nullptr),
      _targetAngle(0.0f),
      _baseBody(nullptr),
      _targetBody(nullptr),
      _pathClear(false),
      _aligned(false),
      _anchorStart(b2Vec2_zero),
      _arrowsLeft(0),
      _framesPerShot(0),
      _frameCount(0),
      _retargetCount(0),
      _firingAllowed(false),
      _unlimitedArrows(false),
      _arrowGunType(0),
      _currentAngle(0.0f)
{
}

// @00580288 (D1), @005802e4 (D0)
ArrowGun::~ArrowGun()
{
}

// @00580308
bool ArrowGun::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);
    _baseBody = groupBody;
    _pathClear = false;
    _aligned = false;
    _firingAllowed = true;
    _unlimitedArrows = false;
    _arrowsLeft = 10;
    _framesPerShot = 3;
    _frameCount = 0;
    _retargetCount = 0;

    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &angle);
    getLevel()->convertPositionAndRotationData(&x, &y, &angle);
    bool fixed = true;
    int rateOfFire = 1;
    element->boolAttribute("p3", &fixed);
    element->intAttribute("p4", &rateOfFire);
    element->boolAttribute("p5", &_dontShootPlayer);

    _framesPerShot = fmax(46 - rateOfFire * 4, 6.0);

    Vec2 position((groupOffset.x + x) * getPtm(), (groupOffset.y + y) * getPtm());
    _mc = Node::create();
    _mc->setPosition(position);
    _mc->setRotation(angle);

    float ptm = getPtm();
    Sprite* base = Sprite::createWithSpriteFrameName("arrow_gun_base.png");
    base->setTag(1);
    base->setPosition(Vec2(0.5f, ptm * (8.6f / globals::flash::ptmRatio) + 0.5f));

    _turret = Sprite::createWithSpriteFrameName("arrow_gun_turret.png");
    _turret->setAnchorPoint(Vec2(0.5f, 0.08f));
    _turret->setRotation(angle);
    _turret->setPosition(Vec2(0.5f, ptm * 0.432f));
    _mc->addChild(base);
    _mc->addChild(_turret);
    getLevelItemsNode()->addChild(_mc);

    _stringMC = Sprite::createWithSpriteFrameName("arrow_gun_string_1.png");
    _stringMC->setPosition(Vec2(_turret->getContentSize().width * 0.5f,
                                _turret->getContentSize().height * 0.45f));
    _turret->addChild(_stringMC, -1);

    _currentAngle = angle * -0.0174532924f;
    createBody(b2Vec2(groupOffset.x + x, groupOffset.y + y), _currentAngle, fixed);

    addToBeginContact(_rangeSensor);
    addToEndContact(_rangeSensor);
    getLevel()->addToActions(this);
    getLevel()->addToFrameActions(this);
    getLevel()->addToPaintItem(this);
    return true;
}

// @005808ec
void ArrowGun::createBody(b2Vec2 position, float angle, bool fixed)
{
    b2PolygonShape shape;
    b2FixtureDef fixtureDef;
    fixtureDef.friction = 0.5f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 0.0f;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = -20;

    if (fixed) {
        // Base and range sensor on the level body (density and group as left above).
        shape.SetAsBox(0.224f, 0.128f, position, angle);
        fixtureDef.shape = &shape;
        getLevelBody()->CreateFixture(&fixtureDef);
        fixtureDef.isSensor = true;
        shape.SetAsBox(6.4f, 3.2f,
                       b2Vec2(position.x + sinf(angle) * -3.2f, position.y + cosf(angle) * 3.2f), angle);
        _rangeSensor = getLevelBody()->CreateFixture(&fixtureDef);
        _turretPos = getLevelBody()->GetWorldPoint(
            b2Vec2(position.x + sinf(angle) * -0.432f, position.y - cosf(angle) * -0.432f));
    } else if (_baseBody == nullptr) {
        // Own dynamic body.
        _arrowGunType = 1;
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position = position;
        bodyDef.angle = angle;
        fixtureDef.density = 3.0f;
        _baseBody = getWorld()->CreateBody(&bodyDef);
        shape.SetAsBox(0.224f, 0.128f);
        fixtureDef.filter.groupIndex = 0;
        fixtureDef.shape = &shape;
        _baseBody->CreateFixture(&fixtureDef);
        _baseBody->ResetMassData();

        fixtureDef.isSensor = true;
        fixtureDef.filter.groupIndex = -20;
        fixtureDef.density = 0.0f;
        shape.SetAsBox(6.4f, 3.2f, b2Vec2(0.0f, 3.2f), 0.0f);
        fixtureDef.shape = &shape;
        _rangeSensor = _baseBody->CreateFixture(&fixtureDef);
        _turretLocalPos = b2Vec2(0.0f, 0.432f);
    } else {
        // Attached to the group body passed to init().
        _arrowGunType = 2;
        float s = sinf(angle);
        float c = cosf(angle);
        fixtureDef.density = 3.0f;
        fixtureDef.filter.groupIndex = 0;
        shape.SetAsBox(0.224f, 0.128f, position, angle);
        fixtureDef.shape = &shape;
        _baseBody->CreateFixture(&fixtureDef);

        fixtureDef.density = 0.0f;
        fixtureDef.isSensor = true;
        fixtureDef.filter.groupIndex = -20;
        shape.SetAsBox(6.4f, 3.2f, b2Vec2(position.x - s * 3.2f, position.y + c * 3.2f), angle);
        fixtureDef.density = 0.0f;
        fixtureDef.shape = &shape;
        _rangeSensor = _baseBody->CreateFixture(&fixtureDef);
        _turretLocalPos.x = position.x + s * -0.432f;
        _turretLocalPos.y = position.y - c * -0.432f;
    }
}

// @00580ca8
b2Body* ArrowGun::getJointBody(b2Vec2 point)
{
    return _baseBody;
}

// @00580cb0 (also inlined into actions)
void ArrowGun::findClosestTarget(b2Vec2 position)
{
    float closest = 1000000.0f;
    for (auto it = _targetDictionary.begin(); it != _targetDictionary.end(); ++it) {
        b2Body* body = it->first;
        float distanceSquared = (body->GetWorldCenter() - position).LengthSquared();
        if (distanceSquared < closest) {
            _targetBody = body;
            closest = distanceSquared;
        }
    }
}

// @00580d38
void ArrowGun::targetRemove(b2Fixture* fixture)
{
    b2Body* body = fixture->GetBody();
    if (_targetDictionary.find(body) == _targetDictionary.end()) {
        return;
    }
    unsigned int count = _targetDictionary[body];
    if (count - 1 == 0) {
        _targetDictionary.erase(body);
        if (_targetBody && body == _targetBody) {
            _targetBody = nullptr;
        }
    } else {
        _targetDictionary[body] = count - 1;
    }
}

// @00580f88
void ArrowGun::targetRemove2(b2Fixture* fixture)
{
    targetRemove(fixture);
}

// @00580f8c
void ArrowGun::targetAdd(b2Fixture* fixture)
{
    unsigned int material = getLevel()->getFixtureMaterial(fixture);
    if (material != 0xFFFFFFFF && (material & 2)) {
        _targetDictionary[fixture->GetBody()]++;
    }
}

// @00581078
void ArrowGun::targetAdd2(b2Fixture* fixture)
{
    targetAdd(fixture);
}

// @0058107c
Arrow* ArrowGun::fireArrow()
{
    // Lead the target: solve |center + v*t - turret| = 32 * t (arrow speed 32 m/s).
    b2Body* target = _targetBody;
    b2Vec2 velocity = target->GetLinearVelocity();
    b2Vec2 center = target->GetWorldCenter();
    b2Vec2 delta = center - _turretPos;
    float a = std::pow(velocity.x, 2) + std::pow(velocity.y, 2) - std::pow(32, 2);
    float b = 2 * b2Dot(velocity, delta);
    float c = std::pow(delta.x, 2) + std::pow(delta.y, 2);
    float discriminant = std::pow(b, 2) - 4 * a * c;
    if (discriminant < 0.0f) {
        return nullptr;
    }
    float root = sqrtf(discriminant);
    float t1 = (root - b) / (2 * a);
    float t2 = (-b - root) / (2 * a);
    if (t1 < 0.0f) {
        t1 = 10000.0f;
    }
    if (t2 < 0.0f) {
        t2 = 10000.0f;
    }
    float t = fminf(t1, t2);
    float angle = atan2f(center.y + velocity.y * t - _turretPos.y, center.x + velocity.x * t - _turretPos.x);

    Arrow* arrow = Arrow::createWithPos(_turretPos, angle, b2Vec2(cosf(angle) * 32.0f, sinf(angle) * 32.0f),
                                        _turret->getLocalZOrder());
    arrow->retain();

    unsigned int variant = ceilf(CCRANDOM_0_1() * 2);
    std::string soundName = "ArrowFire" + patch::to_string(variant);
    createPositionSound(soundName, Vec2(_turretPos.x, _turretPos.y), 1.0f, false);
    return arrow;
}

// @00581314
void ArrowGun::advanceStringMC()
{
    if (++_stringMCFrame > 4) {
        _shouldUpdateFrame = false;
        _stringMCFrame = 1;
    }
    if (_stringMC) {
        _stringMC->setSpriteFrame(SpriteFrameCache::getInstance()->getSpriteFrameByName(
            "arrow_gun_string_" + patch::to_string(_stringMCFrame) + ".png"));
    }
}

// @005814d4
void ArrowGun::frameAction()
{
    if (_frameCount != 0) {
        _frameCount--;
    }
    // String release animation after a shot: advance every other frame.
    if (_stringMCFrame > 1) {
        bool shouldUpdate = _shouldUpdateFrame;
        if (shouldUpdate) {
            advanceStringMC();
        }
        _shouldUpdateFrame = !shouldUpdate;
    }
}

// @0058152c
void ArrowGun::actions()
{
    if (_arrowGunType != 0) {
        _turretPos = _baseBody->GetWorldPoint(_turretLocalPos);
    }
    if (!_targetBody && _firingAllowed) {
        findClosestTarget(_turretPos);
    }
    if (!_targetBody) {
        return;
    }

    if (_aligned && _frameCount == 0 && _firingAllowed) {
        TargetRaycast callback;
        callback._closestFraction = 1.0f;
        callback._targetBody = _targetBody;
        getWorld()->RayCast(&callback, _turretPos, _targetBody->GetWorldCenter());
        if (callback._hitBody == _targetBody) {
            Arrow* arrow = fireArrow();
            advanceStringMC();
            if (!_unlimitedArrows) {
                _arrowsLeft--;
            } else {
                if (_arrows.size() > 10) {
                    _arrows[0]->remoteBreak();
                    _arrows.erase(_arrows.begin());
                }
                if (arrow) {
                    _arrows.push_back(arrow);
                }
            }
            _frameCount = _framesPerShot;
            if (_arrowsLeft == 0) {
                getLevel()->removeFromActions(this);
                arrowGunDie();
                b2Body* body = _baseBody;
                if (!body) {
                    body = getLevelBody();
                }
                body->DestroyFixture(_rangeSensor);
                _rangeSensor = nullptr;
                return;
            }
        }
    }

    // Turn the turret towards the target.
    b2Vec2 targetCenter = _targetBody->GetWorldCenter();
    float targetAngle = atan2f(targetCenter.y - _turretPos.y, targetCenter.x - _turretPos.x) - M_PI_2;
    float difference = _currentAngle - targetAngle;
    if (difference > M_PI) {
        difference -= 2 * b2_pi;
    }
    _aligned = false;
    if (difference < -M_PI) {
        difference += 2 * b2_pi;
    }
    if (fabsf(difference) <= 0.52f) {
        _aligned = true;
    }
    _currentAngle = _currentAngle + difference * -0.5f * LevelItem::s_timeStepOverFlashTimeStep;
}

// @005819d8
void ArrowGun::arrowGunDie()
{
    ContactListener* contactListener = getSession()->getContactListener();
    contactListener->removeBeginContactListener(_rangeSensor, this);
    contactListener->removeEndContactListener(_rangeSensor, this);
    _targetDictionary.clear();
    _arrows.clear();
}

// @00581a38
void ArrowGun::paint()
{
    if (_arrowGunType == 1) {
        b2Vec2 center = _baseBody->GetWorldCenter();
        _mc->setPosition(Vec2(center.x * getPtm(), center.y * getPtm()));
        _mc->setRotation(_baseBody->GetAngle() * -57.29578f);
    }
    if (_arrowsLeft != 0) {
        float turretRotation = _currentAngle * -57.29578f;  // not fused with the subtraction
        _turret->setRotation(turretRotation - _mc->getRotation());
    }
}

// @00581b48
void ArrowGun::paintWithOffsetPoints(Vec2 offset, float angleDegrees)
{
    _mc->setPosition(offset);
    _mc->setRotation(angleDegrees);
}

// @00581bc4
void ArrowGun::arrowBroken(Arrow* arrow)
{
    auto it = std::find(_arrows.begin(), _arrows.end(), arrow);
    if (it != _arrows.end()) {
        (*it)->release();
        _arrows.erase(it);
    }
}

// @00581c40
void ArrowGun::setOpacity(float opacity)
{
    Node* base = _mc->getChildByTag(1);
    int alpha = opacity * 255.0f;
    _stringMC->setOpacity(alpha);
    base->setOpacity(alpha);
    _turret->setOpacity(alpha);
}

// @00581cd0
void ArrowGun::removeSprites()
{
    _mc->removeFromParentAndCleanup(false);
}

// @00581ce4
void ArrowGun::stopInteractivity()
{
    _targetBody = nullptr;
    _firingAllowed = false;
    _stringMC = nullptr;
    if (_rangeSensor) {
        removeBeginContact(_rangeSensor);
        removeEndContact(_rangeSensor);
    }
    LevelItem::stopInteractivity();
}

// @00581d2c
void ArrowGun::dealloc()
{
    _arrows.clear();
}

// @00581d38
void ArrowGun::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    targetAdd(otherFixture);
}

// @00581d40
void ArrowGun::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    targetRemove(otherFixture);
}
