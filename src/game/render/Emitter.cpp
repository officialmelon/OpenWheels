#include "Emitter.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "EmitterDelegate.h"
#include "Session.h"
#include "Settings.h"
#include "StageCamera.h"
#include "base/ccMacros.h"
#include "base/ccRandom.h"

USING_NS_CC;

namespace {

// Random starting spin of a particle, in [-360, 360) degrees. The original builds a float in
// [2, 4) directly from the bits of one rand() result and subtracts 3.
// RE-NOTE(@00578330): the helper the original used for this is unknown (no symbol, inlined); the
// integer/bit expression below is exactly what the binary computes.
inline float randomSpinStart()
{
    const unsigned int r = static_cast<unsigned int>(std::rand());
    const unsigned int bits = ((r * 0x40500u + 0x100u) & 0x7fff00u) | 0x40000000u;
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return (f - 3.0f) * 360.0f;
}

// Quad vertices of one particle: cocos2d-x's file-static updatePosWithParticle
// (CCParticleSystemQuad.cpp), inlined into Emitter::updateParticleQuads in the original.
inline void updatePosWithParticle(V3F_C4B_T2F_Quad* quad, const Vec2& newPosition, float size,
                                  float rotation)
{
    GLfloat size_2 = size / 2;
    GLfloat x1 = -size_2;
    GLfloat y1 = -size_2;

    GLfloat x2 = size_2;
    GLfloat y2 = size_2;
    GLfloat x = newPosition.x;
    GLfloat y = newPosition.y;

    GLfloat r = (GLfloat)-CC_DEGREES_TO_RADIANS(rotation);
    GLfloat cr = cosf(r);
    GLfloat sr = sinf(r);
    GLfloat ax = x1 * cr - y1 * sr + x;
    GLfloat ay = x1 * sr + y1 * cr + y;
    GLfloat bx = x2 * cr - y1 * sr + x;
    GLfloat by = x2 * sr + y1 * cr + y;
    GLfloat cx = x2 * cr - y2 * sr + x;
    GLfloat cy = x2 * sr + y2 * cr + y;
    GLfloat dx = x1 * cr - y2 * sr + x;
    GLfloat dy = x1 * sr + y2 * cr + y;

    // bottom-left
    quad->bl.vertices.x = ax;
    quad->bl.vertices.y = ay;

    // bottom-right vertex:
    quad->br.vertices.x = bx;
    quad->br.vertices.y = by;

    // top-left vertex:
    quad->tl.vertices.x = dx;
    quad->tl.vertices.y = dy;

    // top-right vertex:
    quad->tr.vertices.x = cx;
    quad->tr.vertices.y = cy;
}

}  // namespace

// @00577550
bool Emitter::isFinished()
{
    return false;
}

// @00577558
Emitter::Emitter()
    : _emitterDelegate(nullptr)
    , _gameplayPause(false)
    , _ptmRatio(80.0f)
{
    // _unk0x5f8, _maxParticles, _unk0x6a0 and _stageCamera are left uninitialised, as in the
    // original (the colour slots are default-constructed Color4Fs).
    _colors.numberOfColors = 0;
    _startColor = Color4F::WHITE;
}

// @0057760c
Emitter::~Emitter()
{
}

// @00577704
bool Emitter::init(float ptmRatio, int numberOfParticles)
{
    _ptmRatio = ptmRatio;
    _stageCamera = Settings::getInstance()->getCurrentSession()->getCamera();
    bool result = ParticleSystemQuad::initWithTotalParticles(numberOfParticles);
    _duration = DURATION_INFINITY;
    return result;
}

// @00577750
bool Emitter::init(__Array* textures, float ptmRatio, float timeStep, Vec2 gravity)
{
    // textures and timeStep are unused.
    _stageCamera = Settings::getInstance()->getCurrentSession()->getCamera();
    _ptmRatio = ptmRatio;
    _gravity = gravity;
    bool result = ParticleSystemQuad::initWithTotalParticles(_maxParticles);
    setAutoRemoveOnFinish(true);
    return result;
}

// @005777cc
void Emitter::update(float dt)
{
    if (_gameplayPause)
    {
        return;
    }

    updateParticleQuads();
    _transformSystemDirty = false;
    // only update gl buffer when visible
    if (_visible && !_batchNode)
    {
        postStep();
    }

    float limit = _stageCamera->getYParticleLimit();
    // Gravity: 10 m/s^2 over one 1/60 s step, in pixels/s (velocities are per second).
    float gravityStep = _ptmRatio * 0.16666667f;

    // Kill particles that fell below the camera (swap with the last one; the swapped-in
    // particle is not re-tested this frame).
    for (int i = 0; i < _particleCount; ++i)
    {
        if (_particleData.posy[i] < limit)
        {
            _particleData.copyParticle(i, _particleCount - 1);
            --_particleCount;
            if (_particleCount == 0 && _isAutoRemoveOnFinish)
            {
                if (_emitterDelegate != nullptr)
                {
                    _emitterDelegate->emitterComplete(this);
                }
                unscheduleUpdate();
                _parent->removeChild(this, true);
                return;
            }
        }
    }

    // The per-particle velocity is kept in startPosX/startPosY; fixed 1/60 s step.
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData.posx[i] += _particleData.startPosX[i] * (1.0f / 60.0f);
    }
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData.posy[i] += _particleData.startPosY[i] * (1.0f / 60.0f);
    }
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData.startPosY[i] -= gravityStep;
    }
    // Spin is applied twice: once per step unscaled, and once scaled by dt below.
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData.rotation[i] += _particleData.deltaRotation[i];
    }
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData.colorR[i] += dt * _particleData.deltaColorR[i];
    }
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData.colorG[i] += dt * _particleData.deltaColorG[i];
    }
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData.colorB[i] += dt * _particleData.deltaColorB[i];
    }
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData.colorA[i] += dt * _particleData.deltaColorA[i];
    }
    for (int i = 0; i < _particleCount; ++i)
    {
        _particleData.rotation[i] += dt * _particleData.deltaRotation[i];
    }
}

// @00578048
void Emitter::updateParticleQuads()
{
    if (_particleCount <= 0)
    {
        return;
    }

    // Particles live in the parent's (EmitterNode's) space: offset by its position. No batch
    // node / position type handling (unlike ParticleSystemQuad::updateParticleQuads).
    V3F_C4B_T2F_Quad* startQuad = _quads;
    const Vec2& pos = getParent()->getPosition();

    {
        Vec2 newPos;
        float* x = _particleData.posx;
        float* y = _particleData.posy;
        float* s = _particleData.size;
        float* r = _particleData.rotation;
        V3F_C4B_T2F_Quad* quadStart = startQuad;
        for (int i = 0; i < _particleCount; ++i, ++x, ++y, ++quadStart, ++s, ++r)
        {
            newPos.set(*x + pos.x, *y + pos.y);
            updatePosWithParticle(quadStart, newPos, *s, *r);
        }
    }

    // set color
    if (_opacityModifyRGB)
    {
        V3F_C4B_T2F_Quad* quad = startQuad;
        float* r = _particleData.colorR;
        float* g = _particleData.colorG;
        float* b = _particleData.colorB;
        float* a = _particleData.colorA;

        for (int i = 0; i < _particleCount; ++i, ++quad, ++r, ++g, ++b, ++a)
        {
            GLubyte colorR = *r * *a * 255;
            GLubyte colorG = *g * *a * 255;
            GLubyte colorB = *b * *a * 255;
            GLubyte colorA = *a * 255;
            quad->bl.colors.set(colorR, colorG, colorB, colorA);
            quad->br.colors.set(colorR, colorG, colorB, colorA);
            quad->tl.colors.set(colorR, colorG, colorB, colorA);
            quad->tr.colors.set(colorR, colorG, colorB, colorA);
        }
    }
    else
    {
        V3F_C4B_T2F_Quad* quad = startQuad;
        float* r = _particleData.colorR;
        float* g = _particleData.colorG;
        float* b = _particleData.colorB;
        float* a = _particleData.colorA;

        for (int i = 0; i < _particleCount; ++i, ++quad, ++r, ++g, ++b, ++a)
        {
            GLubyte colorR = *r * 255;
            GLubyte colorG = *g * 255;
            GLubyte colorB = *b * 255;
            GLubyte colorA = *a * 255;
            quad->bl.colors.set(colorR, colorG, colorB, colorA);
            quad->br.colors.set(colorR, colorG, colorB, colorA);
            quad->tl.colors.set(colorR, colorG, colorB, colorA);
            quad->tr.colors.set(colorR, colorG, colorB, colorA);
        }
    }
}

// @00578308
void Emitter::setColorArray(Color4F* colors, int count)
{
    for (int i = 0; i < count; ++i)
    {
        _colors.colors[i] = colors[i];
    }
    _colors.numberOfColors = count;
}

// @00578330
void Emitter::addParticle(Vec2 position, Vec2 velocity, float rotationSpeed, float size)
{
    if (_paused)
    {
        return;
    }

    float rotation = randomSpinStart();
    int index = _particleCount++;

    _particleData.posx[index] = position.x;
    _particleData.posy[index] = position.y;
    // startPos holds the particle velocity (pixels per second), see update().
    _particleData.startPosX[index] = velocity.x;
    _particleData.startPosY[index] = velocity.y;
    _particleData.size[index] = size;
    _particleData.deltaSize[index] = 0.0f;
    _particleData.rotation[index] = rotation;
    _particleData.deltaRotation[index] = rotationSpeed;
    _particleData.timeToLive[index] = 9999.0f;
    _particleData.deltaColorA[index] = 0.0f;
    _particleData.deltaColorR[index] = 0.0f;
    _particleData.deltaColorG[index] = 0.0f;
    _particleData.deltaColorB[index] = 0.0f;

    if (_colors.numberOfColors > 0)
    {
        const Color4F& color = _colors.colors[cocos2d::random(0, _colors.numberOfColors - 1)];
        _particleData.colorR[index] = color.r;
        _particleData.colorG[index] = color.g;
        _particleData.colorB[index] = color.b;
        _particleData.colorA[index] = color.a;
    }
    else
    {
        _particleData.colorR[index] = _startColor.r;
        _particleData.colorG[index] = _startColor.g;
        _particleData.colorB[index] = _startColor.b;
        _particleData.colorA[index] = _startColor.a;
    }
}

// @005784ec
void Emitter::setGameplayPause(bool pause)
{
    _gameplayPause = pause;
}

// @005784f4
int Emitter::getMaxParticles()
{
    return _maxParticles;
}

// @005784fc
void Emitter::setEmitterDelegate(EmitterDelegate* emitterDelegate)
{
    _emitterDelegate = emitterDelegate;
}

// @00578504
void Emitter::setCallback(const std::function<void()>& callback)
{
    // FUN_005785f0 is libc++'s std::function swap (operator= is copy-and-swap).
    _callback = callback;
}

// @005785d8
void Emitter::executeCallback()
{
    if (_callback)
    {
        _callback();
    }
}
