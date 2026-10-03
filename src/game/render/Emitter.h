#pragma once

// Emitter: cocos2d::ParticleSystemQuad whose particles are spawned explicitly (addParticle) and
// moved by the game at a fixed 1/60 step: velocity in ParticleData::startPos, gravity
// ptm*10/60 per step, killed below the camera's particle limit. Notifies its EmitterDelegate
// when the last particle dies. Own fields 0x5f8..0x6b0 (arm64).

#include <functional>

#include "2d/CCParticleSystemQuad.h"
#include "base/ccTypes.h"
#include "math/Vec2.h"

namespace cocos2d {
class __Array;
}

class EmitterDelegate;
class StageCamera;

// Random particle colours (iOS original: _particleColorSet {ccColor4B colors[5];
// unsigned numberOfColors}; the port uses Color4F). Name of the type is ours.
struct ParticleColorSet
{
    cocos2d::Color4F colors[5];  // +0x00
    int numberOfColors;          // +0x50 0: use ParticleSystem::_startColor
};

class Emitter : public cocos2d::ParticleSystemQuad
{
public:
    Emitter();
    virtual ~Emitter();

    virtual void update(float dt) override;              // +0x3d8
    virtual void updateParticleQuads() override;         // +0x528

    virtual bool init(float ptmRatio, int numberOfParticles);                  // +0x6c0
    virtual bool init(cocos2d::__Array* textures, float ptmRatio, float timeStep,
                      cocos2d::Vec2 gravity);                                  // +0x6c8
    virtual bool isFinished();                                                 // +0x6d0
    virtual void setEmitterDelegate(EmitterDelegate* emitterDelegate);         // +0x6d8

    void setColorArray(cocos2d::Color4F* colors, int count);
    void addParticle(cocos2d::Vec2 position, cocos2d::Vec2 velocity, float rotationSpeed,
                     float size);
    void setGameplayPause(bool pause);
    int getMaxParticles();
    void setCallback(const std::function<void()>& callback);
    void executeCallback();

protected:
    int _unk0x5f8;                         // +0x5f8 RE-TODO: never accessed
    ParticleColorSet _colors;              // +0x5fc setColorArray / addParticle
    EmitterDelegate* _emitterDelegate;     // +0x650
    bool _gameplayPause;                   // +0x658
    float _ptmRatio;                       // +0x65c
    cocos2d::Vec2 _gravity;                // +0x660 stored by init(..), unused by Emitter::update
    int _maxParticles;                     // +0x668 particle budget (Session::canAddEmitter)
    std::function<void()> _callback;       // +0x670
    void* _unk0x6a0;                       // +0x6a0 RE-TODO: never accessed
    StageCamera* _stageCamera;             // +0x6a8 (iOS name)
};
