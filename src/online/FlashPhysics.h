#pragma once
// ONLINE (PC addition): the browser game's physics profile for converted browser levels.
//
// The browser game (Flash, Box2D 2.0) steps its world once per 30 Hz frame:
// world.Step(1/30, 10) - ten velocity and up to ten position iterations, and a contact solver
// that resolves every contact point on its own (Box2D 2.0 had no 2-point block solver). The
// mobile game (Box2D 2.3) steps at 1/60 with 8 + 3 iterations and the block solver. Stacks,
// balances, "don't move" levels, joint stiffness and every browser replay (one input byte per
// 30 Hz frame) depend on that difference, so with "browser physics" on (QOL page, default off)
// a converted browser level runs:
//   * one world step per Flash frame: the Session's time step is 1/30 (Session::setTimeStep, the
//     original game's own variable-step mechanism: characters rescale their joint limits, items
//     their impulses through LevelItem::s_timeStepOverFlashTimeStep);
//   * 10 / 10 iterations and g_blockSolve off during the step;
//   * the Box2D 2.0 contact rules of FlashRuntime (no polygon skin, no bounding-box wakes).
// Campaign and other mobile levels never see any of it. With the option off, browser levels play
// on the mobile profile (1/60, 8 + 3), as in OpenWheels 0.2.

#include <string>

class b2World;
class LevelB2D;
class Session;

namespace online {

// The QOL option (persisted; default off).
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

}  // namespace online
