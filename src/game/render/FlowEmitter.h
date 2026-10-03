#pragma once

// FlowEmitter: Emitter that keeps spawning one particle per interval from a (moving) body point
// until its budget (_maxParticles) runs out (blood, soda, sparkle flows). sizeof 0x700 (arm64).

#include "Box2D/Box2D.h"
#include "Emitter.h"

class FlowEmitter : public Emitter
{
public:
    FlowEmitter();
    virtual ~FlowEmitter();

    static FlowEmitter* createBloodFlow(float minSpeed, float maxSpeed, int count,
                                        b2Body* targetBody, b2Vec2 offset, float rotation);
    bool init(cocos2d::__Array* textures, float ptmRatio, float timeStep, cocos2d::Vec2 gravity,
              float minSpeed, float maxSpeed, float minSize, float maxSize, float minRotation,
              float maxRotation, int count, b2Body* targetBody, b2Vec2 offset,
              cocos2d::Vec2 startPos, float rotation, float interval);
    static FlowEmitter* createSodaFlow(float minSpeed, float maxSpeed, int count,
                                       b2Body* targetBody, b2Vec2 offset, float rotation);
    static FlowEmitter* createSparkleFlow(cocos2d::Vec2 startPos, float rotation);

    virtual void update(float dt) override;
    virtual void stop() override;          // budget = 0
    virtual bool isFinished() override;    // budget == 0

protected:
    // Names from the iOS original's Flow ivars where they exist.
    float _minSpeed;           // +0x6b0 (* _speedScale)
    float _maxSpeed;           // +0x6b4
    float _minSize;            // +0x6b8
    float _maxSize;            // +0x6bc
    float _minRotation;        // +0x6c0 rotation speed (radians), random sign
    float _maxRotation;        // +0x6c4
    b2Body* _targetBody;       // +0x6c8
    b2Vec2 _offset;            // +0x6d0 local point on _targetBody
    cocos2d::Vec2 _startPos;   // +0x6d8 used when _targetBody is null
    float _rotOffsetRadians;   // +0x6e0 emission angle (+ body angle)
    float _interval;           // +0x6e4 seconds between particles (Android-only names below)
    float _intervalTimer;      // +0x6e8
    float _speedScale;         // +0x6ec ctor constant
    float _timeAccumulator;    // +0x6f0
};
