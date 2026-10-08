#include "Jet.h"

#include <cmath>
#include <functional>
#include <string>

#include "LevelB2D.h"
#include "LevelDataElement.h"
#include "Patch.h"
#include "QueryCallback.h"
#include "Sound.h"
#include "online/FlashPhysics.h"  // ONLINE (PC addition)

USING_NS_CC;

// @005cd3a8 (D0; D1 is LevelItem's)
Jet::~Jet()
{
}

// @005cbf04
bool Jet::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);
    loadSpriteFrames(LevelItemTextureIdMineExplosion);

    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;
    bool sleeping = false;
    float power = 0.0f;
    int fireSeconds = 0;
    int accelSeconds = 0;
    bool fixedRotation = false;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &angle);
    getLevel()->convertPositionAndRotationData(&x, &y, &angle);
    element->boolAttribute("p3", &sleeping);
    element->floatAttribute("p4", &power);
    element->intAttribute("p5", &fireSeconds);
    element->intAttribute("p6", &accelSeconds);
    element->boolAttribute("p7", &fixedRotation);

    _mc = Sprite::createWithSpriteFrameName("jet_body.png");
    _lights = Sprite::createWithSpriteFrameName("jet_spin.png");
    _flames = Sprite::createWithSpriteFrameName("jet_flame.png");

    float ptm = getPtm();
    static float lightsOffset = ptm * -0.0056f;
    static float flamesOffset = ptm * -0.304f;
    static float halfHeight = _mc->getTextureRect().size.height * 0.5f;
    static float halfWidth = _mc->getTextureRect().size.width * 0.5f;

    _lights->setPosition(Vec2(halfWidth, halfHeight + lightsOffset));
    _flames->setPosition(Vec2(halfWidth, halfHeight + flamesOffset));
    _flames->setAnchorPoint(Vec2(0.5f, 1.0f));
    _mc->addChild(_flames, -1);
    _mc->addChild(_lights, 1);
    _mc->setAnchorPoint(Vec2(0.5f, 0.58f));
    _mc->setScale(power / 10.0f * 0.5f + 0.5f);
    _mc->setRotation(angle);
    _mc->setPosition(Vec2(ptm * x, ptm * y));
    getLevelItemsNode()->addChild(_mc);

    createBody(b2Vec2(x, y), angle, sleeping, fixedRotation ? 1.0f : 0.0f, power);

    // ONLINE (PC addition): computed per jet, not once per program run (static in the original):
    // browser levels step at 1/30 (online/FlashPhysics.h), the campaign at 1/60.
    const int fps = (int)roundf(1.0f / LevelItem::s_timeStep);

    _power = power;
    float accelScaler = 1.0f;
    float accelStep = 0.0f;
    if (accelSeconds > 0) {
        accelScaler = 0.0f;
        // ONLINE (PC addition): the ramp advances once per thrust, i.e. once per Flash frame
        // (every other step at 1/60, every step at 1/30), so its step is the 1/60 one at both rates.
        const int rampFps = fps * 2 / online::stepsPerFlashFrame();
        accelStep = 1.0f / (float)(rampFps * accelSeconds);
        _fadeTime = (float)accelSeconds;
    }
    _accelScaler = accelScaler;
    _accelStep = accelStep;
    _firingAllowed = true;
    _skipFrame = false;
    _fireTotal = fireSeconds > 0 ? fps * fireSeconds : -1;

    addToPostSolve(_jetShape);
    if (!sleeping) {
        fireEngine();
    } else {
        _flames->setVisible(false);
        _lights->setVisible(false);
    }
    getLevel()->addToFrameActions(this);
    return true;
}

// @005cc6a0
void Jet::createBody(b2Vec2 position, float angleDegrees, bool sleeping, float fixedRotation, float power)
{
    // The collision box grows with the power (1..10).
    float sizeScale = (power - 1.0f) / 9.0f;
    b2PolygonShape shape;
    shape.SetAsBox((sizeScale * 0.248f + 0.248f) * 0.5f, (sizeScale * 0.248f + 0.304f) * 0.5f);

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.friction = 0.5f;
    fixtureDef.restitution = 0.1f;
    fixtureDef.density = 3.0f;
    fixtureDef.filter.categoryBits = 0x0003;
    fixtureDef.filter.maskBits = 0xFFFF;
    fixtureDef.filter.groupIndex = 0;

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.angle = angleDegrees * -0.0174532924f;
    bodyDef.awake = !sleeping;
    bodyDef.fixedRotation = fixedRotation != 0.0f;
    bodyDef.userData = _mc;

    _body = getWorld()->CreateBody(&bodyDef);
    _jetShape = _body->CreateFixture(&fixtureDef);
    _body->ResetMassData();
    getLevel()->addToPaintBody(_body);
    _smashImpulse = (int)roundf(_body->GetMass() * 100.0f);
}

// @005cc858 (also inlined into init/frameAction)
void Jet::fireEngine()
{
    _firing = true;
    _flames->setVisible(true);
    _lights->setVisible(true);
}

// @005cc8a0
void Jet::postSolve(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact,
                    const b2ContactImpulse* impulse)
{
    float maxImpulse = impulse->normalImpulses[0];
    if (contact->GetManifold()->pointCount == 2) {
        maxImpulse = b2Max(maxImpulse, impulse->normalImpulses[1]);
    }
    if (maxImpulse > (float)_smashImpulse) {
        removePostSolve(_jetShape);
        getLevel()->addToSingleActions(this);
    }
}

// @005cc90c
void Jet::singleAction()
{
    explode();
}

// @005cc910
void Jet::explode()
{
    blastBodies(_body->GetPosition(), 2.6f);
    getLevel()->removeFromFrameActions(this);
    getLevel()->removeFromPaintBody(_body);

    Vector<SpriteFrame*> frames(40);
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    for (int i = 1; i <= 40; i++) {
        frames.pushBack(cache->getSpriteFrameByName("mineExplosion_" + patch::to_string(i) + ".png"));
    }
    Animate* animate = Animate::create(Animation::createWithSpriteFrames(frames, 1.0f / 60.0f, 1));

    Node* parent = _mc->getParent();
    b2Vec2 position = _body->GetPosition();
    Vec2 pixelPosition(position.x * getPtm(), position.y * getPtm());
    float angle = _body->GetAngle();
    _explosion = Sprite::createWithSpriteFrameName("mineExplosion_1.png");
    _explosion->setScale(2.0f);
    _explosion->setAnchorPoint(Vec2(0.5f, 0.04f));
    _explosion->setPosition(pixelPosition);
    _explosion->setRotation(angle * -57.29578f);
    parent->addChild(_explosion);
    _explosion->runAction(Sequence::create(
        animate, CallFunc::create(std::bind(&Jet::explosionAnimationComplete, this)), nullptr));

    createPositionSound("MineExplosion", Vec2(position.x, position.y), 1.0f, false);

    getWorld()->DestroyBody(_body);
    _body = nullptr;
    stopSoundLoop();
    _mc->removeFromParentAndCleanup(false);
    _mc = nullptr;
    _flames = nullptr;
    _lights = nullptr;
}

// @005cce4c
void Jet::blastBodies(b2Vec2 center, float radius)
{
    QueryCallback callback;
    b2AABB aabb;
    aabb.lowerBound = b2Vec2(center.x - radius, center.y - radius);
    aabb.upperBound = b2Vec2(center.x + radius, center.y + radius);
    getWorld()->QueryAABB(&callback, aabb);

    for (unsigned int i = 0; i < callback._fixtures.size(); i++) {
        b2Body* body = callback._fixtures[i]->GetBody();
        if (body->GetType() == b2_staticBody) {
            continue;
        }
        b2Vec2 bodyCenter = body->GetWorldCenter();
        b2Vec2 delta = bodyCenter - center;
        float blastAngle = atan2f(delta.y, delta.x);
        b2Vec2 direction(cosf(blastAngle), sinf(blastAngle));
        float falloff = 1.0f - fminf(radius, delta.Length()) / radius;
        b2Vec2 impulse = falloff * direction;
        impulse = 10.0f * impulse;
        body->ApplyLinearImpulse(impulse, bodyCenter, true);
    }
}

// @005cd048
void Jet::explosionAnimationComplete()
{
    _explosion->removeFromParent();
    _explosion = nullptr;
}

// @005cd078 (also inlined into explode/frameAction)
void Jet::stopSoundLoop()
{
    if (_soundLoop) {
        _soundLoop->stop();
        _soundLoop = nullptr;
    }
}

// @005cd0ac
void Jet::frameAction()
{
    if (!_body || !_body->IsAwake()) {
        return;
    }
    if (!_firingAllowed) {
        if (_firing) {
            _firing = false;
            _flames->setVisible(false);
            _lights->setVisible(false);
            stopSoundLoop();
        }
        if (_accelScaler != 0.0f) {
            // (sic) scaler - scaler: the thrust ramp drops to 0 at once.
            _accelScaler = fmaxf(_accelScaler - _accelScaler, 0.0f);
        }
        return;
    }

    if (!_firing) {
        fireEngine();
    }
    if (_fireTotal >= 0 && ++_fireCount > (unsigned int)_fireTotal) {
        _flames->setVisible(false);
        _lights->setVisible(false);
        getLevel()->removeFromFrameActions(this);
        stopSoundLoop();
    }

    if (_skipFrame) {
        _skipFrame = false;
        return;
    }
    // ONLINE (PC addition): every other step is once per Flash frame at 1/60; with the browser
    // physics profile (1/30) every step is one, so none is skipped.
    _skipFrame = online::stepsPerFlashFrame() == 2;

    // Thrust along the jet's local +y axis, every other step.
    b2Body* body = _body;
    _accelScaler = fminf(_accelStep + _accelScaler, 1.0f);
    float angle = body->GetAngle();
    b2Vec2 impulse = (_accelScaler * (float)_power) * b2Vec2(-sinf(angle), cosf(angle));
    body->ApplyLinearImpulse(impulse, body->GetWorldCenter(), true);

    _lights->setScaleX(-_lights->getScaleX());
    _flames->setScaleX(CCRANDOM_0_1() * 0.4f + 0.8f);
    _flames->setScaleY(CCRANDOM_0_1() * 0.2f + 0.9f);
}

// @005cd368
b2Body* Jet::getJointBody(b2Vec2 point)
{
    return _body;
}

// @005cd370
void Jet::die()
{
    if (_explosion) {
        _explosion->removeFromParentAndCleanup(true);
        _explosion = nullptr;
    }
}
