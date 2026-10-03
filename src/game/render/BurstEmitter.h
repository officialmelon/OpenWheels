#pragma once

// BurstEmitter: Emitter that spawns all its particles at once in onEnter (blood, bottle glass,
// wood chips, crackers, cart parts, mine debris, van glass). sizeof 0x6f0 (arm64).

#include "Box2D/Box2D.h"
#include "Emitter.h"

class BurstEmitter : public Emitter
{
public:
    BurstEmitter();
    virtual ~BurstEmitter();

    static BurstEmitter* createBloodBurst(float initialRange, float speedRange,
                                          cocos2d::Vec2 startPos, int count);
    bool init(cocos2d::__Array* textures, float ptmRatio, float timeStep, cocos2d::Vec2 gravity,
              float minSize, float maxSize, float minRotation, float maxRotation,
              float initialRange, float speedRange, b2Body* targetBody, b2Vec2 offset,
              cocos2d::Vec2 startPos, int count);
    static BurstEmitter* createBloodBurst(float initialRange, float speedRange, b2Body* targetBody,
                                          b2Vec2 offset, int count);
    static BurstEmitter* createBottleBurst(b2Body* body, cocos2d::Color4F color);
    static BurstEmitter* createWoodchipBurst(b2Body* body, cocos2d::Vec2 localPoint);
    static BurstEmitter* createCrackerBurst(b2Body* body, cocos2d::Vec2 localPoint);
    static BurstEmitter* createCartBurst(cocos2d::Vec2 position);
    static BurstEmitter* createMineBurst(b2Body* body, cocos2d::Vec2 localPoint);
    static BurstEmitter* createVanGlassBurst(b2Body* body, cocos2d::Vec2 localPoint);

    virtual void onEnter() override;          // ParticleSystem::onEnter + createParticles
    void createParticles();
    virtual void update(float dt) override;   // Emitter::update at most once per 1/60 s

protected:
    // Names from the iOS original's Burst/Flow ivars where they exist.
    float _minSize;            // +0x6b0
    float _maxSize;            // +0x6b4
    float _minRotation;        // +0x6b8 rotation speed, random sign
    float _maxRotation;        // +0x6bc
    float _initialRange;       // +0x6c0 positions jittered by +-range/2
    float _speedRange;         // +0x6c4
    b2Body* _targetBody;       // +0x6c8 spawn at a body point (velocity inherited), else _startPos
    b2Vec2 _offset;            // +0x6d0 local point on _targetBody
    cocos2d::Vec2 _startPos;   // +0x6d8
    float _timeAccumulator;    // +0x6e0 (Android-only; name ours)
};
