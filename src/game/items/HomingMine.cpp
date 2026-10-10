#include "HomingMine.h"

#include <cmath>
#include <functional>
#include <string>

#include "BurstEmitter.h"
#include "EmitterNode.h"
#include "Globals.h"
#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"
#include "QueryCallback.h"
#include "Session.h"
#include "Sound.h"
#include "TargetRaycast.h"
#include "online/FlashPhysics.h"  // ONLINE (PC addition)
#include "online/RenderInterpolation.h"  // ONLINE (PC addition)

USING_NS_CC;

// Range sensor radius in metres (iOS ivar sensorRadius; a TU static on Android, dynamic init).
static float s_sensorRadius = 600.0f / globals::flash::ptmRatio;  // @00ac61fc

// @005c3034
HomingMine::HomingMine()
    : _mc(nullptr),
      _light(nullptr),
      _jet1(nullptr),
      _jet2(nullptr),
      _jet3(nullptr),
      _jet4(nullptr),
      _explosion(nullptr),
      _mineBody(nullptr),
      _target(nullptr),
      _mineShape(nullptr),
      _rangeSensor(nullptr),
      _inContact(false),
      _hoverLoop(nullptr),
      _beepLoop(nullptr)
{
    // _explosionDistance, _lightColor, _pathClear, _countingDown, _skipAFrame, _counter,
    // _retargetCount, _seekSpeed, _totalImpulseVector and _mineBodyMassInv are left
    // uninitialised, as in the original (the factory does not zero-fill either).
}

// @005c3094 (D1), @005c30d0 (D0)
HomingMine::~HomingMine()
{
}

// @005c30f4
bool HomingMine::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);
    loadSpriteFrames(LevelItemTextureIdMineExplosion);

    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    getLevel()->convertPositionAndRotationData(&x, &y, &rotation);

    float ptm = getPtm();
    _countingDown = false;
    const float ptmRatio = globals::flash::ptmRatio;
    float explosionDistance = 30.0f / ptmRatio;
    _seekSpeed = 0.01f;
    _explosionDistance = explosionDistance * explosionDistance;
    element->floatAttribute("p2", &_seekSpeed);
    _seekSpeed = _seekSpeed * 0.01f;
    element->floatAttribute("p3", &_counter);

    Node* levelItemsNode = getLevelItemsNode();
    _mc = Sprite::createWithSpriteFrameName("homingMine.png");
    _mc->setPosition(Vec2(ptm * x, ptm * y));
    _light = Sprite::createWithSpriteFrameName("homingMine_green.png");
    _light->setPosition(Vec2(_mc->getContentSize().width * 0.5f, _mc->getContentSize().height * 0.5f));
    _mc->addChild(_light);

    // Four thrusters around the mine (up, right, down, left), scaled by paint().
    Vec2 jetAnchor(1.125f, 0.5f);
    float width = _mc->getContentSize().width;
    float height = _mc->getContentSize().height;
    _jet1 = Sprite::createWithSpriteFrameName("homingMine_jet.png");
    float halfWidth = width * 0.5f;
    float halfHeight = height * 0.5f;
    float jetOffset = ptm * (7.0f / ptmRatio);
    _jet1->setAnchorPoint(jetAnchor);
    _jet1->setPosition(Vec2(halfWidth, jetOffset + halfHeight));
    _jet1->setRotation(90.0f);
    _jet2 = Sprite::createWithSpriteFrameName("homingMine_jet.png");
    _jet2->setAnchorPoint(jetAnchor);
    _jet2->setPosition(Vec2(jetOffset + halfWidth, halfHeight));
    _jet2->setRotation(180.0f);
    _jet3 = Sprite::createWithSpriteFrameName("homingMine_jet.png");
    _jet3->setAnchorPoint(jetAnchor);
    _jet3->setPosition(Vec2(halfWidth, halfHeight - jetOffset));
    _jet3->setRotation(270.0f);
    _jet4 = Sprite::createWithSpriteFrameName("homingMine_jet.png");
    _jet4->setAnchorPoint(jetAnchor);
    _jet4->setPosition(Vec2(halfWidth - jetOffset, halfHeight));
    _mc->addChild(_jet1, -1);
    _mc->addChild(_jet2, -2);
    _mc->addChild(_jet3, -3);
    _mc->addChild(_jet4, -4);
    levelItemsNode->addChild(_mc);

    createBodies(b2Vec2(x, y));
    addToBeginContact(_rangeSensor);
    addToEndContact(_rangeSensor);
    addToPostSolve(_mineShape);
    getLevel()->addToActions(this);
    getLevel()->addToFrameActions(this);

    b2Vec2 velocity = _mineBody->GetLinearVelocity();
    velocity.y = velocity.y + 10.0f / LevelItem::s_timeStepInverse;
    _mineBody->SetLinearVelocity(velocity);
    return true;
}

// @005c37c0
void HomingMine::createBodies(b2Vec2 position)
{
    b2World* world = getWorld();

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.allowSleep = false;

    b2CircleShape circle;
    circle.m_radius = 10.0f / globals::flash::ptmRatio;
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &circle;
    fixtureDef.friction = 1.0f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 1.0f;
    fixtureDef.filter.categoryBits = 0x0008;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = 0;

    _mineBody = world->CreateBody(&bodyDef);
    _mineShape = _mineBody->CreateFixture(&fixtureDef);
    _mineBody->ResetMassData();

    // Range sensor around the mine.
    fixtureDef.isSensor = true;
    fixtureDef.density = 0.0f;
    fixtureDef.filter.groupIndex = -20;
    circle.m_radius = s_sensorRadius;
    _mineBodyMassInv = 1.0f / _mineBody->GetMass();
    _rangeSensor = _mineBody->CreateFixture(&fixtureDef);
    getLevel()->addToPaintItem(this);
    _mineBody->SetLinearVelocity(b2Vec2(0.0f, 10.0f / LevelItem::s_timeStepInverse));
}

// @005c3964 (also inlined into actions)
void HomingMine::findClosestTarget()
{
    _retargetCount = 0.0f;
    if (_targetDictionary.empty()) {
        return;
    }
    float closest = 1000000.0f;
    b2Vec2 position = _mineBody->GetPosition();
    for (auto it = _targetDictionary.begin(); it != _targetDictionary.end(); ++it) {
        b2Body* body = it->first;
        float distanceSquared = (body->GetWorldCenter() - position).LengthSquared();
        if (distanceSquared < closest) {
            _target = body;
            closest = distanceSquared;
        }
    }
}

// @005c39f8
void HomingMine::explode()
{
    if (!_mineBody) {
        return;
    }
    b2World* world = getWorld();
    setLightColor(HomingMineLightColorNone);
    b2Vec2 position = _mineBody->GetPosition();
    float angle = _mineBody->GetAngle();
    EmitterNode* particles = getSession()->getParticlesForeground();
    if (particles) {
        getPtm();  // result unused in the original
        Emitter* burst = BurstEmitter::createMineBurst(_mineBody, Vec2::ZERO);
        if (burst) {
            particles->addChild(burst);
        }
    }
    world->DestroyBody(_mineBody);
    _mineBody = nullptr;

    Node* parent = _mc->getParent();
    _mc->removeFromParentAndCleanup(false);
    killBeepLoop();
    killHoverLoop();
    getLevel()->removeFromPaintItem(this);
    getLevel()->removeFromActions(this);
    getLevel()->removeFromFrameActions(this);
    blastBodies(position, 2.6f);

    Vector<SpriteFrame*> frames(40);
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    for (int i = 1; i <= 40; i++) {
        frames.pushBack(cache->getSpriteFrameByName("mineExplosion_" + patch::to_string(i) + ".png"));
    }
    Animate* animate = Animate::create(Animation::createWithSpriteFrames(frames, 1.0f / 60.0f, 1));

    Vec2 pixelPosition(position.x * getPtm(), position.y * getPtm());
    _explosion = Sprite::createWithSpriteFrameName("mineExplosion_1.png");
    _explosion->setScale(2.0f);
    _explosion->setAnchorPoint(Vec2(0.5f, 0.04f));
    _explosion->setPosition(pixelPosition);
    _explosion->setRotation(angle);  // (sic) radians passed as degrees
    parent->addChild(_explosion);
    _explosion->runAction(
        Sequence::create(animate, CallFunc::create(std::bind(&HomingMine::explosionComplete, this)), nullptr));

    createPositionSound("MineExplosion", Vec2(position.x, position.y), 1.0f, false);
}

// @005c3f94
void HomingMine::setLightColor(HomingMineLightColor color)
{
    if (_lightColor == color) {
        return;
    }
    _lightColor = color;
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    SpriteFrame* frame;
    switch (_lightColor) {
    case HomingMineLightColorRed:
        frame = cache->getSpriteFrameByName("homingMine_red.png");
        break;
    case HomingMineLightColorGreen:
        frame = cache->getSpriteFrameByName("homingMine_green.png");
        break;
    case HomingMineLightColorYellow:
        frame = cache->getSpriteFrameByName("homingMine_yellow.png");
        break;
    case HomingMineLightColorNone:
        return;
    }
    _light->setSpriteFrame(frame);
}

// @005c4100 (also inlined into explode)
void HomingMine::killBeepLoop()
{
    if (_beepLoop) {
        _beepLoop->stop();
        _beepLoop = nullptr;
    }
}

// @005c4134 (also inlined into explode)
void HomingMine::killHoverLoop()
{
    if (_hoverLoop) {
        _hoverLoop->stop();
        _hoverLoop = nullptr;
    }
}

// @005c4168
void HomingMine::blastBodies(b2Vec2 center, float radius)
{
    QueryCallback callback;
    b2AABB aabb;
    aabb.lowerBound = b2Vec2(center.x - radius, center.y - radius);
    aabb.upperBound = b2Vec2(center.x + radius, center.y + radius);
    online::flashQueryAABB(getWorld(), &callback, aabb);  // ONLINE (PC addition)

    for (unsigned int i = 0; i < callback._fixtures.size(); i++) {
        b2Fixture* fixture = callback._fixtures[i];
        b2Body* body = fixture->GetBody();
        if (body->GetType() == b2_staticBody) {
            continue;
        }
        b2Vec2 bodyCenter = body->GetWorldCenter();
        b2Vec2 delta = bodyCenter - center;
        float falloff = 1.0f - fminf(radius, delta.Length()) / radius;
        float blastAngle = atan2f(delta.y, delta.x);
        b2Vec2 direction(cosf(blastAngle), sinf(blastAngle));
        b2Vec2 impulse = falloff * direction;
        impulse = 10.0f * impulse;
        body->ApplyLinearImpulse(impulse, bodyCenter, true);
        LevelItem* item = static_cast<LevelItem*>(fixture->GetUserData());
        if (item) {
            item->explodeShape(fixture, falloff);
        }
    }
}

// @005c44e0
void HomingMine::explosionComplete()
{
    _explosion->removeFromParent();
    _explosion = nullptr;
}

// @005c45a0
void HomingMine::frameAction()
{
    // Blink the light every other frame while the fuse is running.
    if (_skipAFrame) {
        _skipAFrame = false;
        return;
    }
    _skipAFrame = true;
    // ONLINE (PC addition): every other 1/60 step = every browser physics step (1/30).
    if (online::stepsPerFlashFrame() == 1) _skipAFrame = false;
    if (_lightColor == HomingMineLightColorYellow) {
        _light->setVisible(!_light->isVisible());
    }
}

// @005c4610
void HomingMine::actions()
{
    if (!_mineBody) {
        return;
    }
    if (_countingDown) {
        _counter = _counter - LevelItem::s_timeStep;
        if (_counter <= 0.0f) {
            explode();
            return;
        }
    }

    b2Vec2 position = _mineBody->GetPosition();
    b2Vec2 velocity = _mineBody->GetLinearVelocity();
    float angularVelocity = _mineBody->GetAngularVelocity();
    if (!_target) {
        findClosestTarget();
    }
    float seek = _seekSpeed * LevelItem::s_timeStepOverFlashTimeStep;
    float damping = LevelItem::s_timeStepOverFlashTimeStep * 0.5f;

    b2Vec2 impulse;
    if (!_target) {
        // Nothing in range: brake.
        impulse = b2Vec2(-(velocity.x * damping) / _mineBodyMassInv, -(velocity.y * damping) / _mineBodyMassInv);
        if (_hoverLoop) {
            _hoverLoop->fadeTo(0.0f, 0.5f, true);
            _hoverLoop = nullptr;
            createPositionSound("HomingMineLose", Vec2(position.x * getPtm(), position.y * getPtm()), 1.0f, false);
        }
    } else {
        b2Vec2 toTarget = _target->GetWorldCenter() - position;
        float distanceSquared = toTarget.LengthSquared();
        if (!_countingDown && (distanceSquared < _explosionDistance || _inContact)) {
            if (!(_counter > 0.0f)) {  // (NaN explodes too, as in the original's b.le)
                explode();
                return;
            }
            setLightColor(HomingMineLightColorYellow);
            _countingDown = true;
            _beepLoop = createBodySound("HomingMineBeep", _mineBody, 1.0f, true);
        }

        TargetRaycast callback;
        callback._closestFraction = 1.0f;
        callback._targetBody = _target;
        getWorld()->RayCast(&callback, _mineBody->GetPosition(), _target->GetWorldCenter());
        bool steer = false;
        if (callback._hitBody == _target) {
            steer = _pathClear;
        } else {
            _pathClear = false;
        }

        if (steer) {
            if (!_countingDown) {
                setLightColor(HomingMineLightColorRed);
            }
            if (!_hoverLoop) {
                _hoverLoop = createBodySound("MineHover", _mineBody, 1.0f, true);
                createBodySound("HomingMineFind", _mineBody, 1.0f, false);
            }
            // Steer: cancel the velocity component across the target direction, spend the rest
            // of the seek impulse along it.
            b2Vec2 step = LevelItem::s_timeStep * velocity;
            b2Vec2 direction = toTarget;
            direction.Normalize();
            float along = step.y * direction.x;
            float cross = along - step.x * direction.y;
            if (fabsf(cross) > seek) {
                if (cross > 0.0f) {
                    impulse = b2Cross(direction, seek);
                } else {
                    impulse = seek * b2Vec2(-direction.y, direction.x);
                }
            } else {
                float forward = sqrtf(cross * cross + seek * seek);
                impulse = b2Cross(direction, cross) + forward * direction;
            }
        } else {
            // Target hidden (or just found): brake.
            impulse = b2Vec2(-(velocity.x * damping) / _mineBodyMassInv, -(velocity.y * damping) / _mineBodyMassInv);
            if (!_countingDown) {
                setLightColor(HomingMineLightColorGreen);
            }
            if (_hoverLoop) {
                _hoverLoop->fadeTo(0.0f, 0.5f, true);
                _hoverLoop = nullptr;
                createPositionSound("HomingMineLose", Vec2(position.x * getPtm(), position.y * getPtm()), 1.0f, false);
            }
        }

        _retargetCount += LevelItem::s_timeStep;
        if (_retargetCount >= 1.0f / 6.0f) {
            findClosestTarget();
        }
    }

    // Apply: angular damping, the steering impulse and the hover lift (cancels gravity).
    _pathClear = true;
    b2Body* body = _mineBody;
    float inverseInertia = 1.0f / body->GetInertia();
    float angularImpulse = -(angularVelocity * damping) / inverseInertia;
    body->SetAngularVelocity(angularVelocity + angularImpulse * inverseInertia);
    b2Vec2 newVelocity(velocity.x + impulse.x * _mineBodyMassInv,
                       10.0f / LevelItem::s_timeStepInverse + (velocity.y + impulse.y * _mineBodyMassInv));
    body->SetLinearVelocity(newVelocity);

    _totalImpulseVector.x = impulse.x;
    _totalImpulseVector.y = impulse.y + seek * -0.5;
}

// @005c4dac
void HomingMine::singleAction()
{
    getLevel()->removeFromActions(this);
    explode();
}

// @005c4dd8
void HomingMine::paint()
{
    b2Vec2 center = _mineBody->GetWorldCenter();
    _mc->setPosition(Vec2(center.x * getPtm(), center.y * getPtm()));
    float angle = _mineBody->GetAngle();
    _mc->setRotation(angle * -57.29578f);
    // ONLINE (PC addition): the frames drawn between browser physics steps only move the mine;
    // the jets' flicker draws rand() and stays once per step (online/RenderInterpolation.h).
    if (online::interp::drawing()) {
        return;
    }

    // Thrust in the mine's frame, with some flicker, drives the four jet sprites.
    float impulseX = _totalImpulseVector.x;
    float s = sinf(angle);
    float c = cosf(angle);
    float localX = impulseX * c + _totalImpulseVector.y * s;
    float localY = impulseX * s + _totalImpulseVector.y * c;
    localX = localX + (CCRANDOM_0_1() * localX - localX * 0.5f) * 0.5f;
    localY = localY + (CCRANDOM_0_1() * localY - localY * 0.5f) * 0.5f;

    float jet1 = 0.0f;
    if (localY < 0.0f) {
        jet1 = fminf(-localY / _seekSpeed, 2.0f);
    }
    float jet2 = 0.0f;
    if (localX < 0.0f) {
        jet2 = fminf(-localX / _seekSpeed, 2.0f);
    }
    float jet3 = 0.0f;
    if (localY > 0.0f) {
        jet3 = fminf(localY / _seekSpeed, 2.0f);
    }
    float jet4 = 0.0f;
    if (localX > 0.0f) {
        jet4 = fminf(localX / _seekSpeed, 2.0f);
    }
    _jet1->setScaleX(jet1);
    _jet2->setScaleX(jet2);
    _jet3->setScaleX(jet3);
    _jet4->setScaleX(jet4);
    _jet1->setScaleY(jet1);
    _jet2->setScaleY(jet2);
    _jet3->setScaleY(jet3);
    _jet4->setScaleY(jet4);
}

// @005c5024
void HomingMine::triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties)
{
    singleAction();
}

// @005c5030
void HomingMine::beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
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

// @005c529c
void HomingMine::endContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact)
{
    b2Body* body = otherFixture->GetBody();
    if (_targetDictionary.find(body) == _targetDictionary.end()) {
        return;
    }
    unsigned int count = _targetDictionary[body];
    if (count - 1 == 0) {
        _targetDictionary.erase(body);
        if (_target && body == _target) {
            _target = nullptr;
        }
    } else {
        _targetDictionary[body] = count - 1;
    }
}

// @005c54ec
void HomingMine::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                           const b2ContactImpulse* impulse)
{
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        maxImpulse = b2Max(maxImpulse, impulse->normalImpulses[1]);
    }
    LevelB2D* level = getLevel();
    if (maxImpulse > 3.0f) {
        level->addToSingleActions(this);
        removePostSolve(_mineShape);
        return;
    }
    if (level->getFixtureMaterial(otherFixture) & 2) {
        _inContact = true;
    }
}
