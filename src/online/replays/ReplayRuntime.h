#pragma once
// ONLINE (PC addition): recording the player's runs of online levels as browser replays and
// watching browser replays, on top of the game's own replay mode (Gameplay with a ReplayData).
// Not part of the 1:1 reconstruction; only active in browser levels (online::flashLevel()).
//
// Timing: a browser replay byte drives one 30 Hz Flash frame = two 1/60 s steps here. The game's
// own replay entries are per display frame, which drifts against the physics whenever a frame
// does or doesn't step; for browser replays the control byte is therefore picked by physics step:
//   * Gameplay::update calls gameplayState() (replay: the byte for the next step; play: the
//     byte the player pressed is remembered),
//   * Session::update calls physicsStep() before every world step (play: records that byte;
//     replay: fires recorded click triggers).
// Recorded clicks / roll-outs of click triggers (Trigger::onlineMouseClick / onlineMouseMove) go
// into the Flash mouse entries.
//
// Browser replays are input-only, so playback is a re-simulation. With browser physics on
// (online/FlashPhysics.h: 30 Hz steps, Box2D 2.0 solver rules) it follows the browser game
// closely but not bit for bit (float vs the Flash game's doubles, the reconstructed game logic);
// with it off, it is labelled approximate.

#include <functional>
#include <string>
#include <vector>

#include "online/OnlineLevel.h"
#include "online/replays/FlashReplay.h"

class ReplayData;
class Trigger;

namespace online {
namespace replays {

// One run of the player in an online level (this program run only, until saved).
struct RunRecord {
    OnlineLevelInfo level;
    int character = 1;                 // browser character id
    std::vector<uint8_t> steps;        // mobile control byte per world step
    int stepsPerFrame = 2;             // world steps per 30 Hz frame (online/FlashPhysics.h)
    std::vector<MouseEntry> mouse;     // Flash iterations
    bool completed = false;
    int completeStep = 0;              // steps done when the finish line was reached
    bool tooLong = false;              // passed 200 s (the browser's replay limit)
    int serial = 0;
    SavedRun saved;                    // filled once saved / uploaded (file, uploadedId)

    int frames() const;                // 30 Hz frames of the replay
    ReplayInput toInput() const;       // Flash bytes (frame f = the step f*stepsPerFrame byte)
    SavedRun toSavedRun() const;
};

// --- game hooks ---------------------------------------------------------------------------------
void gameplayState(ReplayData* data, bool isReplay, unsigned char* state);
void physicsStep();
void noteClick(Trigger* trigger);
void noteRollOut(Trigger* trigger);

// --- browser ------------------------------------------------------------------------------------
// The level just started from the browser (after startConvertedLevel): runs of it get recorded.
void beginOnlineRun(const OnlineLevelInfo& level);
// Recent runs of a level (0 = all levels), newest first (at most a few are kept).
std::vector<RunRecord*> recentRuns(int levelId);
RunRecord* runBySerial(int serial);

// Watches a replay: converts the level, starts it with the replay's character in replay mode.
// `title` labels the overlay ("Chrepuhon", "Your run"...). False + error when it can't start.
bool watch(const OnlineLevelInfo& level, const ReplayInfo& replay, const ReplayInput& input,
           const std::string& flashXml, const std::string& title, std::string* error);
// --- testing (--online-test, online/account/TjfTestDriver) --------------------------------------
// Record mode: the control byte for world step `step` instead of the player's.
void setTestInput(std::function<uint8_t(int step)> input);
// Called once when `step` world steps are done in a recorded or watched session (mode: 1 record,
// 2 watch); with kObserveEveryStep, before every world step.
constexpr int kObserveEveryStep = -2;
// Fast-forward: every display frame takes one world step whatever the frame's real time
// (Gameplay::update), so a replay plays as fast as the game can draw it.
void setTestFastForward(bool on);
bool testFastForward();
void setTestStepObserver(int step, std::function<void(int mode)> observer);

// Browser character id -> the character this build plays it with (restored or fallback).
int playableCharacter(int browserCharacter);

}  // namespace replays
}  // namespace online
