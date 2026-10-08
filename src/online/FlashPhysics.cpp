// ONLINE (PC addition): see FlashPhysics.h.
#include "online/FlashPhysics.h"

#include <Box2D/Box2D.h>

#include <algorithm>
#include <cmath>

#include "cocos2d.h"
#include "LevelItem.h"
#include "Session.h"
#include "online/FlashRuntime.h"

// Box2D 2.3.1's switch for the 2-point block solver (b2ContactSolver.cpp, not in its headers).
// Every v3-deps-158 prebuilt (win32, linux, mac, ios, android) exports it.
extern bool g_blockSolve;

namespace online {
namespace {

const char* const kOptionKey = "qol_browser_physics";
const float kMobileTimeStep = 1.0f / 60.0f;
const float kFlashTimeStep = 1.0f / 30.0f;
const int kFlashIterations = 10;

bool g_active = false;  // the running level uses the browser profile

// LevelItem keeps the previous step for CharacterB2D::timeStepChanged's ratio; setting the same
// step twice leaves current == previous, i.e. "no change pending".
void settle(Session* session, float timeStep)
{
    if (session) {
        session->setTimeStep(timeStep);
        session->setTimeStep(timeStep);
    } else {
        LevelItem::setTimeStep(timeStep);
        LevelItem::setTimeStep(timeStep);
    }
}

}  // namespace

bool browserPhysicsOption()
{
    return cocos2d::UserDefault::getInstance()->getBoolForKey(kOptionKey, true);
}

void setBrowserPhysicsOption(bool on)
{
    cocos2d::UserDefault::getInstance()->setBoolForKey(kOptionKey, on);
}

bool browserPhysics() { return g_active && flashLevel(); }

int stepsPerFlashFrame() { return browserPhysics() ? 1 : 2; }

namespace {
// Current step / (1/60): exactly 1 at 1/60 (so callers return their argument unchanged).
float stepsRatio60()
{
    const float ratio = LevelItem::s_timeStep * 60.0f;
    return std::fabs(ratio - 1.0f) < 1e-4f ? 1.0f : ratio;
}
}  // namespace

int stepsFor60HzFrames(int frames60)
{
    const float ratio = stepsRatio60();
    if (ratio == 1.0f || frames60 <= 0) return frames60;
    // ratio is 2 (+ float error) at 1/30: halves round up (the 1e-3 absorbs the error).
    return std::max(1, (int)std::floor(frames60 / ratio + 0.5f + 1e-3f));
}

int stepsForFlashFrames(int frames30) { return stepsFor60HzFrames(frames30 * 2); }

float perStep(float per60HzStepValue)
{
    const float ratio = stepsRatio60();
    return ratio == 1.0f ? per60HzStepValue : per60HzStepValue * ratio;
}

void resetLevelTimeStep(Session* session)
{
    if (!g_active) return;
    g_active = false;
    settle(session, kMobileTimeStep);
}

void beginLevelTimeStep(Session* session)
{
    if (!flashLevel() || !browserPhysicsOption()) return;
    g_active = true;
    settle(session, kFlashTimeStep);
}

void flashWorldStep(b2World* world, float timeStep)
{
    if (!browserPhysics()) {
        world->Step(timeStep, 8, 3);
        return;
    }
    const bool blockSolve = g_blockSolve;
    g_blockSolve = false;
    world->Step(timeStep, kFlashIterations, kFlashIterations);
    g_blockSolve = blockSolve;
}

}  // namespace online
