#include "HarpoonGun.h"

#include <cmath>
#include <new>
#include <string>

#include "Globals.h"
#include "Harpoon.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "LevelItemsDrawNode.h"
#include "Patch.h"
#include "Session.h"
#include "Sound.h"
#include "TargetRaycast.h"

USING_NS_CC;

// Harpoon muzzle speed in m/s (dynamic initialiser _INIT_9 @005c2fe4).
static float s_harpoonSpeed = 2000.0f / globals::flash::ptmRatio;  // @00ac61dc

// Inline in the original (only visible inlined into create @005d07d4): zeroes every member but
// _anchorStart.
HarpoonGun::HarpoonGun()
    : _mc(nullptr),
      _useAnchor(false),
      _turret(nullptr),
      _harpoon(nullptr),
      _anchorSprite(nullptr),
      _harpoonSprite(nullptr),
      _rangeSensor(nullptr),
      _targetSensor(nullptr),
      _targetAngle(0.0f),
      _targetLength(0.0f),
      _baseBody(nullptr),
      _turretBody(nullptr),
      _turretJoint(nullptr),
      _targetBody(nullptr),
      _pathClear(false),
      _aligned(false),
      _jointLength(0.0f),
      _fixedTurret(false),
      _turretAngle(0.0f),
      _triggerFiring(false),
      _disabled(false)
{
}

// @005c2f44 (D2), @005c2f90 (D0)
HarpoonGun::~HarpoonGun()
{
}

// @005d07d4
HarpoonGun* HarpoonGun::create(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    HarpoonGun* harpoonGun = new (std::nothrow) HarpoonGun();
    if (harpoonGun) {
        if (harpoonGun->init(element, groupBody, groupOffset)) {
            harpoonGun->autorelease();
        } else {
            delete harpoonGun;
            harpoonGun = nullptr;
        }
    }
    return harpoonGun;
}

// @005c0cb4
bool HarpoonGun::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);
    _harpoon = nullptr;

    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &angle);
    getLevel()->convertPositionAndRotationData(&x, &y, &angle);
    _useAnchor = true;
    _pathClear = false;
    _aligned = false;
    element->boolAttribute("p3", &_useAnchor);
    _fixedTurret = false;
    element->boolAttribute("p4", &_fixedTurret);
    float turretAngle = 0.0f;
    element->floatAttribute("p5", &turretAngle);
    _triggerFiring = false;
    _turretAngle = turretAngle * -0.0174532924f;
    element->boolAttribute("p6", &_triggerFiring);
    _disabled = false;
    element->boolAttribute("p7", &_disabled);

    Vec2 position(x * getPtm(), y * getPtm());
    _mc = Node::create();
    _mc->setPosition(position);
    _mc->setRotation(angle);

    float ptm = getPtm();
    Sprite* base = Sprite::createWithSpriteFrameName("harpoon_base.png");
    const float ptmRatio = globals::flash::ptmRatio;
    base->setPosition(Vec2(0.5f, base->getTextureRect().size.height / 2 + 0.5f + ptm * (-16.0f / ptmRatio)));

    _turret = Sprite::createWithSpriteFrameName("harpoon_turret.png");
    Size turretSize = _turret->getTextureRect().size;
    _turret->setAnchorPoint(Vec2(0.5f, ptm * (6.85f / ptmRatio) / turretSize.height));
    _turret->setPosition(Vec2(0.0f, ptm * (35.0f / ptmRatio)));
    _turret->setRotation(turretAngle);

    // The loaded harpoon, shown in the turret until fired.
    _harpoonSprite = Sprite::createWithSpriteFrameName("harpoon.png");
    _harpoonSprite->setRotation(-90.0f);
    _harpoonSprite->setPosition(Vec2(turretSize.width / 2,
                                     _harpoonSprite->getTextureRect().size.height / 2 + ptm * (50.0f / ptmRatio)));
    _turret->addChild(_harpoonSprite, -1);
    _mc->addChild(base);
    _mc->addChild(_turret);
    getLevelItemsNode()->addChild(_mc);

    setLight();
    angle = angle * -0.0174532924f;
    createBody(b2Vec2(x, y), angle);

    addToBeginContact(_rangeSensor);
    addToEndContact(_rangeSensor);
    getLevel()->addToActions(this);
    return true;
}

// @005c12dc
void HarpoonGun::setLight()
{
    Node* oldLight = _turret->getChildByTag(1);
    if (oldLight) {
        oldLight->removeFromParentAndCleanup(false);
    }
    unsigned int lightType;
    if (_disabled) {
        lightType = 4;
    } else if (_triggerFiring) {
        lightType = 3;
    } else if (_fixedTurret) {
        lightType = 2;
    } else {
        lightType = 1;
    }
    std::string frameName = "harpoon_light_" + patch::to_string(lightType) + ".png";
    Sprite* light = Sprite::createWithSpriteFrameName(frameName);
    light->setTag(1);
    light->setAnchorPoint(Vec2(0.0f, 0.0f));
    light->setPosition(Vec2(7.0f / globals::flash::ptmRatio * getPtm(), 34.0f / globals::flash::ptmRatio * getPtm()));
    _turret->addChild(light);
}

// @005c1514
void HarpoonGun::createBody(b2Vec2 position, float angle)
{
    const float ptmRatio = globals::flash::ptmRatio;

    b2PolygonShape shape;
    b2FixtureDef fixtureDef;
    fixtureDef.friction = 0.5f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 0.0f;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = -20;

    // Base on the level body.
    shape.SetAsBox(25.0f / ptmRatio, 16.0f / ptmRatio, position, angle);
    fixtureDef.shape = &shape;
    getLevelBody()->CreateFixture(&fixtureDef);

    // Range sensor: 800 x 400 px box in front of the gun.
    fixtureDef.isSensor = true;
    float s = sinf(angle);
    float c = cosf(angle);
    shape.SetAsBox(400.0f / ptmRatio, 200.0f / ptmRatio,
                   b2Vec2(position.x - s * 200.0f / ptmRatio, position.y + c * 200.0f / ptmRatio), angle);
    _rangeSensor = getLevelBody()->CreateFixture(&fixtureDef);
    b2Vec2 farCorner = static_cast<b2PolygonShape*>(_rangeSensor->GetShape())->m_vertices[0];

    // Turret body, pinned to the level body.
    position.x = position.x - s * 35.0f / ptmRatio;
    position.y = position.y + c * 35.0f / ptmRatio;
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.angle = angle;
    _turretBody = getWorld()->CreateBody(&bodyDef);

    b2RevoluteJointDef jointDef;
    jointDef.collideConnected = true;
    jointDef.motorSpeed = 0.0f;
    jointDef.maxMotorTorque = 10000.0f;
    _targetLength = (farCorner - position).Length() + 0.5f;
    jointDef.Initialize(_turretBody, getLevelBody(), position);
    _turretJoint = static_cast<b2RevoluteJoint*>(getWorld()->CreateJoint(&jointDef));

    if (_useAnchor) {
        _anchorStart = _turretBody->GetWorldPoint(b2Vec2(-21.5f / ptmRatio, -17.0f / ptmRatio));
        createAnchor();
        getSession()->getBackgroundDrawNode()->addHarpoonGun(this);
    }

    if (_fixedTurret) {
        // A fixed turret only looks straight ahead.
        getLevelBody()->DestroyFixture(_rangeSensor);
        _rangeSensor = nullptr;
        if (!_triggerFiring) {
            createTargetSensor(_targetLength, b2Vec2_zero, 0.0f);
            _pathClear = true;
            _aligned = true;
            getLevel()->addToActions(this);
        }
    }
    if (_fixedTurret || _triggerFiring) {
        _turretBody->SetTransform(_turretBody->GetPosition(), _turretAngle + angle);
    }
}

// @005c194c
void HarpoonGun::die()
{
    if (_harpoon) {
        _harpoon->release();
        _harpoon = nullptr;
    }
}

// @005c1978
void HarpoonGun::targetAdd(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    b2Body* body = otherFixture->GetBody();
    if (getLevel()->getFixtureMaterial(body->GetFixtureList()) & 2) {
        if (_targetDictionary[body] == 0) {
            _targetDictionary[body] = 1;
        } else {
            _targetDictionary[body] = _targetDictionary[body] + 1;
        }
    }
}

// @005c1be4
void HarpoonGun::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    if (fixture == _rangeSensor) {
        targetAdd(fixture, otherFixture, contact);
    } else if (fixture == _targetSensor) {
        targetAdd2(fixture, otherFixture, contact);
    }
}

// @005c1c38
void HarpoonGun::targetAdd2(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    targetAdd(fixture, otherFixture, contact);
    _aligned = true;
}

// @005c1c60
void HarpoonGun::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    targetRemove(fixture, otherFixture, contact);
}

// @005c1c64
void HarpoonGun::targetRemove(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    b2Body* body = otherFixture->GetBody();
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

// @005c1eb4 (also inlined into createBody)
void HarpoonGun::createAnchor()
{
    createAnchorWithEndBody(_turretBody, b2Vec2(0.0f, 50.0f / globals::flash::ptmRatio), 0.0f);
}

// @005c1edc (also inlined into createBody)
void HarpoonGun::createTargetSensor(float length, b2Vec2 offset, float angle)
{
    b2FixtureDef fixtureDef;
    fixtureDef.friction = 0.2f;
    fixtureDef.restitution = 0.0f;
    fixtureDef.density = 1.0f;
    fixtureDef.isSensor = true;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = -20;

    b2PolygonShape shape;
    float ptm = getPtm();
    shape.SetAsBox(2.0f / ptm, length * 0.5f, b2Vec2(0.0f, length * 0.5f), 0.0f);
    fixtureDef.shape = &shape;
    _turretBody->SetType(b2_staticBody);
    _targetSensor = _turretBody->CreateFixture(&fixtureDef);
    addToBeginContact(_targetSensor);
    addToEndContact(_targetSensor);
    _pathClear = false;
}

// @005c1ff8
void HarpoonGun::createAnchorWithEndBody(b2Body* endBody, b2Vec2 endLocalPoint, float length)
{
    // Rope: 7 small circle bodies between _anchorStart (level body) and the end body, joined by 8
    // distance joints.
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;

    b2CircleShape circle;
    circle.m_radius = 2.5f / globals::flash::ptmRatio;
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &circle;
    fixtureDef.friction = 0.5f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 0.25f;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = -20;

    b2Vec2 position = _anchorStart;
    b2Vec2 end = endBody->GetWorldPoint(endLocalPoint);
    b2World* world = getWorld();
    b2Vec2 delta = end - position;

    b2DistanceJointDef jointDef;
    _jointLength = delta.Length() * 1.25f * 0.125f;

    b2Body* previousBody = nullptr;
    for (int i = 0; i < 8; i++) {
        b2Vec2 anchorA;
        if (i == 0) {
            previousBody = getLevelBody();
            anchorA = position;
        } else {
            anchorA = previousBody->GetWorldCenter();
        }
        b2Body* body;
        b2Vec2 anchorB;
        if (i == 7) {
            body = endBody;
            anchorB = end;
        } else {
            position += 0.125f * delta;
            bodyDef.position = position;
            body = world->CreateBody(&bodyDef);
            body->CreateFixture(&fixtureDef);
            body->ResetMassData();
            anchorB = position;
        }
        jointDef.Initialize(previousBody, body, anchorA, anchorB);
        jointDef.length = length > 0.0f ? length * 0.125f : _jointLength;
        _anchorJoints.push_back(static_cast<b2DistanceJoint*>(world->CreateJoint(&jointDef)));
        previousBody = body;
    }
}

// @005c23c0
void HarpoonGun::removeSensors()
{
    if (_rangeSensor) {
        removeBeginContact(_rangeSensor);
        removeEndContact(_rangeSensor);
        getLevelBody()->DestroyFixture(_rangeSensor);
        _rangeSensor = nullptr;
    }
    if (_targetSensor) {
        removeBeginContact(_targetSensor);
        removeEndContact(_targetSensor);
        _turretBody->DestroyFixture(_targetSensor);
        _targetSensor = nullptr;
    }
    _turretJoint->SetMotorSpeed(0.0f);
    _turretJoint->EnableMotor(true);
}

// @005c244c (also inlined into fireHarpoon/fireHarpoon2)
void HarpoonGun::destroyAnchor()
{
    // Destroys the rope bodies (the last joint's body B is the end body and is kept).
    b2World* world = getWorld();
    for (unsigned int i = 0; i < _anchorJoints.size() - 1; i++) {
        world->DestroyBody(_anchorJoints[i]->GetBodyB());
    }
    _anchorJoints.clear();
}

// @005c24bc
std::vector<b2DistanceJoint*> HarpoonGun::getJoints()
{
    return _anchorJoints;
}

// @005c25b4 (also inlined into actions)
void HarpoonGun::findClosestTarget(b2Vec2 position)
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

// @005c263c
void HarpoonGun::fireHarpoon()
{
    _disabled = true;

    // Lead the target (same intercept solve as ArrowGun::fireArrow).
    b2Body* target = _targetBody;
    float speed = s_harpoonSpeed;
    b2Vec2 velocity = target->GetLinearVelocity();
    b2Vec2 origin = _turretBody->GetPosition();
    b2Vec2 center = target->GetWorldCenter();
    b2Vec2 delta = center - origin;
    float a = std::pow(velocity.x, 2) + std::pow(velocity.y, 2) - std::pow(speed, 2);
    float b = 2 * b2Dot(velocity, delta);
    float c = std::pow(delta.x, 2) + std::pow(delta.y, 2);
    float discriminant = std::pow(b, 2) - 4 * a * c;
    if (discriminant < 0.0f) {
        return;
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
    b2Vec2 aim(center.x + velocity.x * t - origin.x, center.y + velocity.y * t - origin.y);
    float angle = atan2f(aim.y, aim.x);

    _harpoon = Harpoon::create(origin, angle, b2Vec2(speed * cosf(angle), speed * sinf(angle)),
                               _turret->getLocalZOrder());
    _harpoon->setHarpoonGun(this);
    _harpoon->retain();
    _harpoonSprite->removeFromParentAndCleanup(false);

    b2Body* harpoonBody = _harpoon->getHarpoonBody();
    Sound* sound = createPositionSound("HarpoonFire", Vec2(harpoonBody->GetPosition().x, harpoonBody->GetPosition().y),
                                       1.0f, false);
    if (sound) {
        sound->setMaxVolume(0.75f);
    }
    if (_useAnchor) {
        destroyAnchor();
        createAnchorWithEndBody(_harpoon->getHarpoonBody(), b2Vec2(-20.0f / globals::flash::ptmRatio, 0.0f),
                                aim.Length());
    }
    removeSensors();
}

// @005c293c
void HarpoonGun::fireHarpoon2()
{
    // Straight along the turret axis.
    b2Body* turretBody = _turretBody;
    b2Vec2 origin = turretBody->GetPosition();
    b2Vec2 direction = turretBody->GetWorldPoint(b2Vec2(0.0f, 10.0f)) - origin;
    float angle = atan2f(direction.y, direction.x);
    float speed = s_harpoonSpeed;

    _harpoon = Harpoon::create(origin, angle, b2Vec2(speed * cosf(angle), speed * sinf(angle)),
                               _turret->getLocalZOrder());
    _harpoon->setHarpoonGun(this);
    _harpoon->retain();

    b2Body* harpoonBody = _harpoon->getHarpoonBody();
    Sound* sound = createPositionSound("HarpoonFire", Vec2(harpoonBody->GetPosition().x, harpoonBody->GetPosition().y),
                                       1.0f, false);
    if (sound) {
        sound->setMaxVolume(0.75f);
    }
    _targetBody = nullptr;
    _targetDictionary.clear();
    if (_useAnchor) {
        destroyAnchor();
        createAnchorWithEndBody(_harpoon->getHarpoonBody(), b2Vec2(-20.0f / globals::flash::ptmRatio, 0.0f),
                                direction.Length());
    }
    removeSensors();
}

// @005c2b9c
void HarpoonGun::harpoonHit(Harpoon* harpoon)
{
    // The harpoon stuck: tighten the rope and make its links heavy.
    if (_useAnchor) {
        harpoon->setHarpoonGun(nullptr);
        for (unsigned int i = 0; i < _anchorJoints.size() - 1; i++) {
            b2DistanceJoint* joint = _anchorJoints[i];
            b2Body* body = joint->GetBodyB();
            joint->SetLength(_jointLength);
            body->GetFixtureList()->SetDensity(10.0f);
            body->ResetMassData();
        }
    }
}

// @005c2c24
void HarpoonGun::actions()
{
    if (_disabled) {
        return;
    }
    b2Vec2 turretPosition = _turretBody->GetPosition();
    if (!_targetBody) {
        findClosestTarget(turretPosition);
        return;
    }

    if (_aligned && !_triggerFiring) {
        TargetRaycast callback;
        callback._closestFraction = 1.0f;
        callback._targetBody = _targetBody;
        getWorld()->RayCast(&callback, _turretBody->GetWorldCenter(), _targetBody->GetWorldCenter());
        if (callback._hitBody == _targetBody) {
            if (!_fixedTurret) {
                fireHarpoon();
            } else {
                fireHarpoon2();
            }
            getLevel()->removeFromActions(this);
        }
    }

    if (!_fixedTurret) {
        b2Vec2 targetCenter = _targetBody->GetWorldCenter();
        float targetAngle = atan2f(targetCenter.y - turretPosition.y, targetCenter.x - turretPosition.x) - M_PI_2;
        _aligned = false;
        float turretAngle = _turretBody->GetAngle();
        float difference = turretAngle - targetAngle;
        if (difference > M_PI) {
            difference -= 2 * b2_pi;
        }
        if (difference < -M_PI) {
            difference += 2 * b2_pi;
        }
        if (fabsf(difference) <= 0.52f) {
            _aligned = true;
        }
        _turretBody->SetTransform(turretPosition,
                                  turretAngle + difference * -0.5f * LevelItem::s_timeStepOverFlashTimeStep);
        _turretBody->SetAngularVelocity(0.0f);
        _turretBody->SetLinearVelocity(b2Vec2(0.0f, 0.0f));

        float rotation = _turretBody->GetAngle() * -57.29578f;  // not fused with the subtraction
        _turret->setRotation(rotation - _mc->getRotation());
    }
}

// @005c2ec8
void HarpoonGun::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    if (action == 2) {
        _disabled = false;
    } else if (action == 1) {
        _disabled = true;
    } else {
        if (action == 0 && !_harpoon && !_disabled) {
            fireHarpoon2();
            removeFromActions();
        }
        return;
    }
    setLight();
}
