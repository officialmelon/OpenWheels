#include "FlowEmitter.h"

#include "qol/QoL.h"  // QOL (PC addition)

#include <cmath>
#include <cstdlib>

#include "Globals.h"
#include "Session.h"
#include "Settings.h"
#include "base/ccMacros.h"
#include "base/ccRandom.h"

USING_NS_CC;

// @005790b8
FlowEmitter::FlowEmitter()
    : _minSpeed(0.0f)
    , _maxSpeed(0.0f)
    , _minSize(0.0f)
    , _maxSize(0.0f)
    , _minRotation(0.0f)
    , _maxRotation(0.0f)
    , _targetBody(nullptr)
    , _offset(b2Vec2_zero)
    , _rotOffsetRadians(0.0f)
    , _interval(0.0f)
    , _intervalTimer(0.0f)
    , _speedScale(4.0f)
    , _timeAccumulator(0.0f)
{
    _maxParticles = 0;
}

// @00579134
FlowEmitter::~FlowEmitter()
{
}

// @005791bc
FlowEmitter* FlowEmitter::createBloodFlow(float minSpeed, float maxSpeed, int count,
                                          b2Body* targetBody, b2Vec2 offset, float rotation)
{
    // Unlike the BurstEmitter factories, there is no Session::canAddEmitter check.
    FlowEmitter* emitter = new (std::nothrow) FlowEmitter();
    emitter->setTotalParticles(count);

    Session* session = Settings::getInstance()->getCurrentSession();
    float ptmRatio = session->getPtmRatio();
    float timeStep = session->getTimeStep();
    Vec2 gravity = session->_gravity;

    __Array* textures = __Array::createWithCapacity(4);
    textures->addObject(__String::create("blood_particle_0.png"));
    textures->addObject(__String::create("blood_particle_1.png"));
    textures->addObject(__String::create("blood_particle_2.png"));
    textures->addObject(__String::create("blood_particle_3.png"));

    Sprite* sprite = Sprite::create("images/dot.png");
    Rect rect = sprite->getTextureRect();

    if (!emitter->init(textures, ptmRatio, timeStep, gravity, minSpeed, maxSpeed,
                       rect.size.width * 0.75f, rect.size.width, 0.0f, 1.0f, count, targetBody,
                       offset, Vec2::ZERO, rotation, 0.033f))
    {
        delete emitter;
        return nullptr;
    }
    emitter->setTexture(sprite->getTexture());
    emitter->setStartColor(Color4F::RED);
    emitter->autorelease();
    qol::markBlood(emitter);  // QOL (PC addition): blood styles
    return emitter;
}

// @0057956c
bool FlowEmitter::init(__Array* textures, float ptmRatio, float timeStep, Vec2 gravity,
                       float minSpeed, float maxSpeed, float minSize, float maxSize,
                       float minRotation, float maxRotation, int count, b2Body* targetBody,
                       b2Vec2 offset, Vec2 startPos, float rotation, float interval)
{
    // Rotations are given in degrees.
    _minSpeed = _speedScale * minSpeed;
    _maxSpeed = _speedScale * maxSpeed;
    _minSize = minSize;
    _maxSize = maxSize;
    _maxParticles = count;
    _minRotation = CC_DEGREES_TO_RADIANS(minRotation);
    _maxRotation = CC_DEGREES_TO_RADIANS(maxRotation);
    _targetBody = targetBody;
    _startPos = startPos;
    _offset = offset;
    _rotOffsetRadians = CC_DEGREES_TO_RADIANS(rotation);
    _interval = interval;
    return Emitter::init(textures, ptmRatio, timeStep, gravity);
}

// @005795d4
FlowEmitter* FlowEmitter::createSodaFlow(float minSpeed, float maxSpeed, int count,
                                         b2Body* targetBody, b2Vec2 offset, float rotation)
{
    // Same as createBloodFlow (same textures), black particles.
    FlowEmitter* emitter = new (std::nothrow) FlowEmitter();
    emitter->setTotalParticles(count);

    Session* session = Settings::getInstance()->getCurrentSession();
    float ptmRatio = session->getPtmRatio();
    float timeStep = session->getTimeStep();
    Vec2 gravity = session->_gravity;

    __Array* textures = __Array::createWithCapacity(4);
    textures->addObject(__String::create("blood_particle_0.png"));
    textures->addObject(__String::create("blood_particle_1.png"));
    textures->addObject(__String::create("blood_particle_2.png"));
    textures->addObject(__String::create("blood_particle_3.png"));

    Sprite* sprite = Sprite::create("images/dot.png");
    Rect rect = sprite->getTextureRect();

    if (!emitter->init(textures, ptmRatio, timeStep, gravity, minSpeed, maxSpeed,
                       rect.size.width * 0.75f, rect.size.width, 0.0f, 1.0f, count, targetBody,
                       offset, Vec2::ZERO, rotation, 0.033f))
    {
        delete emitter;
        return nullptr;
    }
    emitter->setTexture(sprite->getTexture());
    emitter->setStartColor(Color4F::BLACK);
    emitter->autorelease();
    return emitter;
}

// @00579984
FlowEmitter* FlowEmitter::createSparkleFlow(Vec2 startPos, float rotation)
{
    FlowEmitter* emitter = new (std::nothrow) FlowEmitter();
    emitter->setTotalParticles(1000);

    Session* session = Settings::getInstance()->getCurrentSession();
    float ptmRatio = session->getPtmRatio();
    float timeStep = session->getTimeStep();
    Vec2 gravity = session->_gravity;

    __Array* textures = __Array::createWithCapacity(4);
    textures->addObject(__String::create("spark_particle_0.png"));
    textures->addObject(__String::create("spark_particle_1.png"));
    textures->addObject(__String::create("spark_particle_2.png"));

    Sprite* sprite = Sprite::create("images/sparkle.png");
    Rect rect = sprite->getTextureRect();

    if (!emitter->init(textures, ptmRatio, timeStep, gravity, 2.0f, 5.0f, rect.size.width * 1.5f,
                       rect.size.width * 2.5f, 0.0f, 1.0f, 1000, nullptr, b2Vec2_zero, startPos,
                       rotation, 0.033f))
    {
        delete emitter;
        return nullptr;
    }
    emitter->setTexture(sprite->getTexture());

    Color4F colors[5];
    colors[0] = Color4F(globals::colors::blue, 1.0f);
    colors[1] = Color4F(globals::colors::pink, 1.0f);
    colors[2] = Color4F(globals::colors::yellow, 1.0f);
    emitter->setColorArray(colors, 3);
    emitter->autorelease();
    return emitter;
}

// @00579d50
void FlowEmitter::update(float dt)
{
    if (_gameplayPause)
    {
        return;
    }
    // Fixed 1/60 s steps.
    _timeAccumulator += dt;
    if (_timeAccumulator < 1.0f / 60.0f)
    {
        return;
    }
    _timeAccumulator -= 1.0f / 60.0f;

    if (_isActive && _particleCount < (int)(__totalParticleCountFactor * _totalParticles) &&
        _maxParticles > 0)
    {
        _intervalTimer += 1.0f / 60.0f;
        if (_intervalTimer > _interval)
        {
            _intervalTimer = 0.0f;

            Vec2 position = _startPos;
            b2Vec2 velocity = b2Vec2_zero;
            float speed = (_minSpeed + (_maxSpeed - _minSpeed) * CCRANDOM_0_1()) * 60.0f;
            float angle = _rotOffsetRadians;
            if (_targetBody != nullptr)
            {
                // Emit from the body point, inheriting its velocity; the angle turns with it.
                angle += _targetBody->GetAngle();
                velocity = _targetBody->GetLinearVelocityFromLocalPoint(_offset);
                b2Vec2 worldPoint = _targetBody->GetWorldPoint(_offset);
                position = Vec2(worldPoint.x * _ptmRatio, worldPoint.y * _ptmRatio);
            }

            float rotation = 0.0f;
            if (_maxRotation > 0.0f)
            {
                // RE-NOTE(@00579d50): the original draws one random number here and discards it.
                CCRANDOM_0_1();
                float direction = CCRANDOM_0_1();
                float spin = _minRotation + CCRANDOM_0_1() * (_maxRotation - _minRotation);
                rotation = direction > 0.5f ? spin : -spin;
            }

            Vec2 particleVelocity(cosf(angle) * speed + velocity.x * _ptmRatio,
                                  sinf(angle) * speed + velocity.y * _ptmRatio);
            float size = _minSize + (_maxSize - _minSize) * CCRANDOM_0_1();
            addParticle(position, particleVelocity, rotation, size);
            --_maxParticles;
        }
    }
    Emitter::update(dt);
}

// @0057a018
void FlowEmitter::stop()
{
    _maxParticles = 0;
}

// @0057a028
bool FlowEmitter::isFinished()
{
    return _maxParticles == 0;
}
