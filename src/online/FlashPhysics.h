#pragma once
// ONLINE (PC addition): the browser game's physics profile for converted browser levels.
//
// The browser game (Flash, Box2D 2.0) steps its world once per 30 Hz frame:
// world.Step(1/30, 10) - ten velocity and up to ten position iterations, and a contact solver
// that resolves every contact point on its own (Box2D 2.0 had no 2-point block solver). The
// mobile game (Box2D 2.3) steps at 1/60 with 8 + 3 iterations and the block solver. Stacks,
// balances, "don't move" levels, joint stiffness and every browser replay (one input byte per
// 30 Hz frame) depend on that difference, so with "browser physics" on (QOL page, default on)
// a converted browser level runs:
//   * one world step per Flash frame: the Session's time step is 1/30 (Session::setTimeStep, the
//     original game's own variable-step mechanism: characters rescale their joint limits, items
//     their impulses through LevelItem::s_timeStepOverFlashTimeStep);
//   * 10 / 10 iterations and g_blockSolve off during the step;
//   * Box2D's solver switched to the browser game's Box2D 2.0 rules (g_flash20Solver,
//     thirdparty/box2d/README.md): velocity clamps, damping, sleep, contact and joint solvers;
//   * the Box2D 2.0 contact rules of FlashRuntime (no polygon skin, no bounding-box wakes);
//   * smooth drawing: every display frame is drawn between the last two 30 Hz steps
//     (online/RenderInterpolation.h), so it looks like 60 fps or more without changing a step.
// Campaign and other mobile levels never see any of it. With the option off, browser levels play
// on the mobile profile (1/60, 8 + 3), as in OpenWheels 0.2.

#include <string>

class b2World;
class b2QueryCallback;
struct b2AABB;
class LevelB2D;
class Session;

namespace online {

// The QOL option (persisted; default on).
bool browserPhysicsOption();
void setBrowserPhysicsOption(bool on);

// Offline levels (restored campaign, editor levels) are converted from browser format too but
// never use the browser profile: markOfflineLevel tags a converted level's <info>, LevelB2D::addInfo
// reports the tag through setOfflineLevel before beginLevelTimeStep.
std::string markOfflineLevel(const std::string& mobileXml);
void setOfflineLevel(bool offline);

// True while the running level plays with the browser profile (latched when the level starts,
// so toggling the option mid-level waits for the next level).
bool browserPhysics();

// Whether the level being built will play with the browser profile (known before
// beginLevelTimeStep, while the characters are created).
bool browserPhysicsWanted();

// World steps per browser (30 Hz) frame: 1 with the browser profile, 2 on the mobile profile.
int stepsPerFlashFrame();

// Rate helpers for code that counts world steps (any level: they read LevelItem::s_timeStep, so
// campaign levels, always at 1/60, get their original values back exactly).
// A count of 60 Hz steps (a timer written for the mobile game) at the current step, rounded to
// the nearest step (halves up) and at least 1 when frames60 > 0; frames60 itself at 1/60.
int stepsFor60HzFrames(int frames60);
// A count of 30 Hz browser frames (a Flash timer) at the current step; frames30 * 2 at 1/60.
int stepsForFlashFrames(int frames30);
// A per-step increment written for 60 Hz steps (a linear rate, not a factor), at the current step.
float perStep(float per60HzStepValue);

// Session::setupLevel, before the level loads: back to the mobile step for every level.
void resetLevelTimeStep(Session* session);
// LevelB2D::addInfo, once the level knows it is a browser level and its characters exist.
void beginLevelTimeStep(Session* session);

// The world step of a browser level (profile-dependent iterations and solver).
void flashWorldStep(b2World* world, float timeStep);

// LevelB2D: Flash builds the level's shapes and items before its character, and Box2D 2.0 orders
// its bodies, joints and contacts by creation. LevelB2D reads the character first, so it brackets
// the character's creation with these and calls flashLevelBuilt once the level is built: the
// character's bodies and joints move to the front of the lists (the newest) and the fixtures get
// Box2D 2.0's proxy ids in Flash's order.
void flashCharacterBegin(b2World* world);
void flashCharacterEnd(b2World* world);
void flashLevelBuilt(b2World* world);

// b2World::QueryAABB for the browser game's explosions (Mine, HomingMine, Jet, the wheelchair's
// jet): under Box2D 2.0's broad-phase it reports what 2.0's world.Query(aabb, shapes, 30) returns,
// at most 30 fixtures, in 2.0's order.
void flashQueryAABB(b2World* world, b2QueryCallback* callback, const b2AABB& aabb);

}  // namespace online
