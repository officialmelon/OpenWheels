#include "BurstEmitter.h"

#include <cstdlib>

#include "Session.h"
#include "Settings.h"
#include "base/ccMacros.h"
#include "base/ccRandom.h"

USING_NS_CC;

// @00575aa4
BurstEmitter::BurstEmitter()
    : _minSize(0.0f)
    , _maxSize(0.0f)
    , _minRotation(0.0f)
    , _maxRotation(0.0f)
    , _initialRange(0.0f)
    , _speedRange(0.0f)
    , _targetBody(nullptr)
    , _offset(b2Vec2_zero)
    , _timeAccumulator(0.0f)
{
    _maxParticles = 0;
}

// @00575b0c
BurstEmitter::~BurstEmitter()
{
}

// @00575b94
BurstEmitter* BurstEmitter::createBloodBurst(float initialRange, float speedRange, Vec2 startPos,
                                             int count)
{
    if (!Settings::getInstance()->getCurrentSession()->canAddEmitter(count))
    {
        return nullptr;
    }

    BurstEmitter* emitter = new (std::nothrow) BurstEmitter();
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

    if (emitter->init(textures, ptmRatio, timeStep, gravity, rect.size.width * 0.75f,
                      rect.size.width, 0.0f, 0.0f, initialRange, speedRange, nullptr, b2Vec2_zero,
                      startPos, count))
    {
        emitter->setTexture(sprite->getTexture());
        emitter->setStartColor(Color4F::RED);
        emitter->autorelease();
        return emitter;
    }
    delete emitter;
    return nullptr;
}

// @00575f1c
bool BurstEmitter::init(__Array* textures, float ptmRatio, float timeStep, Vec2 gravity,
                        float minSize, float maxSize, float minRotation, float maxRotation,
                        float initialRange, float speedRange, b2Body* targetBody, b2Vec2 offset,
                        Vec2 startPos, int count)
{
    _minSize = minSize;
    _maxSize = maxSize;
    _minRotation = minRotation;
    _maxRotation = maxRotation;
    _initialRange = initialRange;
    _maxParticles = count;
    _speedRange = speedRange;
    _targetBody = targetBody;
    _startPos = startPos;
    _offset = offset;
    return Emitter::init(textures, ptmRatio, timeStep, gravity);
}

// @00575f50
BurstEmitter* BurstEmitter::createBloodBurst(float initialRange, float speedRange,
                                             b2Body* targetBody, b2Vec2 offset, int count)
{
    if (!Settings::getInstance()->getCurrentSession()->canAddEmitter(count))
    {
        return nullptr;
    }

    BurstEmitter* emitter = new (std::nothrow) BurstEmitter();
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

    if (emitter->init(textures, ptmRatio, timeStep, gravity, rect.size.width * 0.75f,
                      rect.size.width, 0.0f, 0.0f, initialRange, speedRange, targetBody, offset,
                      Vec2(0.0f, 0.0f), count))
    {
        emitter->setTexture(sprite->getTexture());
        emitter->setStartColor(Color4F::RED);
        emitter->autorelease();
        return emitter;
    }
    delete emitter;
    return nullptr;
}

// @005762d0
BurstEmitter* BurstEmitter::createBottleBurst(b2Body* body, Color4F color)
{
    if (!Settings::getInstance()->getCurrentSession()->canAddEmitter(30))
    {
        return nullptr;
    }

    BurstEmitter* emitter = new (std::nothrow) BurstEmitter();
    emitter->setTotalParticles(30);

    Session* session = Settings::getInstance()->getCurrentSession();
    float ptmRatio = session->getPtmRatio();
    float timeStep = session->getTimeStep();
    Vec2 gravity = session->_gravity;

    // Left empty (the texture comes from the sprite below), as in the original.
    __Array* textures = __Array::createWithCapacity(1);

    Sprite* sprite = Sprite::create("images/bottleShard.png");
    Rect rect = sprite->getTextureRect();

    if (emitter->init(textures, ptmRatio, timeStep, gravity, rect.size.width,
                      rect.size.width * 3.0f, 0.0f, 12.566371f, 3.0f, 20.0f, body, b2Vec2_zero,
                      Vec2::ZERO, 30))
    {
        emitter->setTexture(sprite->getTexture());
        emitter->setStartColor(color);
        emitter->autorelease();
        return emitter;
    }
    delete emitter;
    return nullptr;
}

// @00576520
BurstEmitter* BurstEmitter::createWoodchipBurst(b2Body* body, Vec2 localPoint)
{
    if (!Settings::getInstance()->getCurrentSession()->canAddEmitter(30))
    {
        return nullptr;
    }

    BurstEmitter* emitter = new (std::nothrow) BurstEmitter();
    emitter->setTotalParticles(30);

    Session* session = Settings::getInstance()->getCurrentSession();
    float ptmRatio = session->getPtmRatio();
    float timeStep = session->getTimeStep();
    Vec2 gravity = session->_gravity;

    __Array* textures = __Array::createWithCapacity(1);

    Sprite* sprite = Sprite::create("images/chip.png");
    Rect rect = sprite->getTextureRect();

    if (emitter->init(textures, ptmRatio, timeStep, gravity, 1.5f * rect.size.width,
                      3.5f * rect.size.width, 0.0f, 12.566371f, 10.0f, 30.0f, body, b2Vec2_zero,
                      localPoint, 30))
    {
        emitter->setTexture(sprite->getTexture());
        emitter->setStartColor(Color4F(Color4B(207, 187, 160, 255)));
        emitter->autorelease();
        return emitter;
    }
    delete emitter;
    return nullptr;
}

// @00576778
BurstEmitter* BurstEmitter::createCrackerBurst(b2Body* body, Vec2 localPoint)
{
    if (!Settings::getInstance()->getCurrentSession()->canAddEmitter(40))
    {
        return nullptr;
    }

    BurstEmitter* emitter = new (std::nothrow) BurstEmitter();
    emitter->setTotalParticles(40);

    Session* session = Settings::getInstance()->getCurrentSession();
    float ptmRatio = session->getPtmRatio();
    float timeStep = session->getTimeStep();
    Vec2 gravity = session->_gravity;

    __Array* textures = __Array::createWithCapacity(1);

    Sprite* sprite = Sprite::create("images/square.png");
    Rect rect = sprite->getTextureRect();

    if (emitter->init(textures, ptmRatio, timeStep, gravity, rect.size.width, rect.size.width,
                      0.0f, 12.566371f, 20.0f, 30.0f, body, b2Vec2_zero, localPoint, 40))
    {
        emitter->setTexture(sprite->getTexture());
        emitter->setStartColor(Color4F(Color4B(185, 161, 128, 255)));
        emitter->autorelease();
        return emitter;
    }
    delete emitter;
    return nullptr;
}

// @005769c4
BurstEmitter* BurstEmitter::createCartBurst(Vec2 position)
{
    if (!Settings::getInstance()->getCurrentSession()->canAddEmitter(20))
    {
        return nullptr;
    }

    BurstEmitter* emitter = new (std::nothrow) BurstEmitter();
    emitter->setTotalParticles(20);

    Session* session = Settings::getInstance()->getCurrentSession();
    float ptmRatio = session->getPtmRatio();
    float timeStep = session->getTimeStep();
    Vec2 gravity = session->_gravity;

    __Array* textures = __Array::createWithCapacity(1);

    Sprite* sprite = Sprite::create("images/cartShard.png");
    Rect rect = sprite->getTextureRect();

    if (emitter->init(textures, ptmRatio, timeStep, gravity, rect.size.width * 0.6f,
                      rect.size.width, 0.0f, 12.566371f, 20.0f, 30.0f, nullptr, b2Vec2_zero,
                      position, 20))
    {
        emitter->setTexture(sprite->getTexture());
        emitter->setStartColor(Color4F(Color4B(102, 102, 102, 255)));
        emitter->autorelease();
        return emitter;
    }
    delete emitter;
    return nullptr;
}

// @00576c1c
BurstEmitter* BurstEmitter::createMineBurst(b2Body* body, Vec2 localPoint)
{
    if (!Settings::getInstance()->getCurrentSession()->canAddEmitter(25))
    {
        return nullptr;
    }

    BurstEmitter* emitter = new (std::nothrow) BurstEmitter();
    emitter->setTotalParticles(25);

    Session* session = Settings::getInstance()->getCurrentSession();
    float ptmRatio = session->getPtmRatio();
    float timeStep = session->getTimeStep();
    Vec2 gravity = session->_gravity;

    __Array* textures = __Array::createWithCapacity(1);

    Sprite* sprite = Sprite::create("images/shard.png");
    Rect rect = sprite->getTextureRect();

    if (emitter->init(textures, ptmRatio, timeStep, gravity, rect.size.width * 0.6f,
                      rect.size.width, 0.0f, 12.566371f, 10.0f, 50.0f, body, b2Vec2_zero,
                      localPoint, 25))
    {
        emitter->setTexture(sprite->getTexture());
        emitter->setStartColor(Color4F(Color4B(102, 102, 102, 255)));
        emitter->autorelease();
        return emitter;
    }
    delete emitter;
    return nullptr;
}

// @00576e74
BurstEmitter* BurstEmitter::createVanGlassBurst(b2Body* body, Vec2 localPoint)
{
    if (!Settings::getInstance()->getCurrentSession()->canAddEmitter(25))
    {
        return nullptr;
    }

    BurstEmitter* emitter = new (std::nothrow) BurstEmitter();
    emitter->setTotalParticles(25);

    Session* session = Settings::getInstance()->getCurrentSession();
    float ptmRatio = session->getPtmRatio();
    float timeStep = session->getTimeStep();
    Vec2 gravity = session->_gravity;

    __Array* textures = __Array::createWithCapacity(1);

    Sprite* sprite = Sprite::create("images/glassShard.png");
    Rect rect = sprite->getTextureRect();

    if (emitter->init(textures, ptmRatio, timeStep, gravity, rect.size.width * 0.5f,
                      rect.size.width, 1.5707964f, 12.566371f, 50.0f, 30.0f, body, b2Vec2_zero,
                      localPoint, 25))
    {
        emitter->setTexture(sprite->getTexture());
        emitter->setStartColor(Color4F(Color4B(28, 28, 108, 128)));
        emitter->autorelease();
        return emitter;
    }
    delete emitter;
    return nullptr;
}

// @005770cc
void BurstEmitter::onEnter()
{
    ParticleSystem::onEnter();
    createParticles();
}

// @005770f0
void BurstEmitter::createParticles()
{
    // Pixel position/velocity the particles start from: a point attached to the target body (its
    // centre of mass plus _offset, unrotated, moving with the body), else the fixed _startPos.
    Vec2 startPosition;
    Vec2 startVelocity(0.0f, 0.0f);
    if (_targetBody != nullptr)
    {
        b2Vec2 localPoint = _targetBody->GetLocalCenter() + _offset;
        b2Vec2 velocity = _targetBody->GetLinearVelocityFromLocalPoint(localPoint);
        b2Vec2 worldPoint = _targetBody->GetWorldCenter() + _offset;
        startPosition = Vec2(worldPoint.x * _ptmRatio, worldPoint.y * _ptmRatio);
        startVelocity = Vec2(velocity.x * _ptmRatio, velocity.y * _ptmRatio);
    }
    else
    {
        startPosition = _startPos;
    }

    float halfRange = _initialRange * 0.5f;
    // Horizontal speeds are centred on 0, vertical ones are biased upwards.
    float halfSpeedRange = _speedRange * 0.5f;
    float quarterSpeedRange = _speedRange * 0.25f;

    for (int i = 0; i < _maxParticles; ++i)
    {
        float x = startPosition.x + CCRANDOM_0_1() * _initialRange - halfRange;
        float y = startPosition.y + CCRANDOM_0_1() * _initialRange - halfRange;
        float vx = CCRANDOM_0_1() * startVelocity.x +
                   CCRANDOM_0_1() * (CCRANDOM_0_1() * _speedRange - halfSpeedRange) * 60.0f;
        float vy = CCRANDOM_0_1() * startVelocity.y +
                   CCRANDOM_0_1() * (CCRANDOM_0_1() * _speedRange - quarterSpeedRange) * 60.0f;
        float size = _minSize + CCRANDOM_0_1() * (_maxSize - _minSize);

        float rotation = 0.0f;
        if (_maxRotation > 0.0f)
        {
            // RE-NOTE(@005770f0): the original draws one random number here and discards it.
            CCRANDOM_0_1();
            float direction = CCRANDOM_0_1();
            float spin = _minRotation + CCRANDOM_0_1() * (_maxRotation - _minRotation);
            rotation = direction > 0.5f ? spin : -spin;
        }

        addParticle(Vec2(x, y), Vec2(vx, vy), rotation, size);
    }
}

// @005773c0
void BurstEmitter::update(float dt)
{
    if (_gameplayPause)
    {
        return;
    }
    // Advance the particles at most once per 1/60 s.
    _timeAccumulator += dt;
    if (_timeAccumulator < 1.0f / 60.0f)
    {
        return;
    }
    _timeAccumulator -= 1.0f / 60.0f;
    Emitter::update(dt);
}
