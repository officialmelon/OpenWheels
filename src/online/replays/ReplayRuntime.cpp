// ONLINE (PC addition): see ReplayRuntime.h.
#include "online/replays/ReplayRuntime.h"

#include <algorithm>
#include <functional>

#include "cocos2d.h"
#include "Gameplay.h"
#include "LevelB2D.h"
#include "LevelSession.h"
#include "LevelStore.h"
#include "ReplayData.h"
#include "Session.h"
#include "Settings.h"
#include "Trigger.h"
#include "online/FlashLevelConverter.h"
#include "online/FlashPhysics.h"
#include "online/FlashRuntime.h"
#include "online/OnlineUi.h"
#include "qol/CharacterChoice.h"  // QOL (PC addition)
#include "restored/Restored.h"

USING_NS_CC;

namespace online {
namespace replays {

namespace {

const size_t kKeptRuns = 6;

enum class Mode { None, Record, Watch };

struct Watching {
    OnlineLevelInfo level;
    ReplayInfo replay;
    ReplayInput input;
    std::string title;
};

// Session state
SessionToken g_token;
Mode g_mode = Mode::None;
int g_steps = 0;                    // world steps done in this session
unsigned char g_frameState = 0;     // control byte of the current display frame (recording)
size_t g_mouseNext = 0;             // next mouse entry to fire (watching)
bool g_finished = false;            // watching: the replay reached the finish line

// Browser state
OnlineLevelInfo g_current;          // the online level last started from the browser
size_t g_currentHash = 0;
bool g_hasCurrent = false;
std::vector<RunRecord*> g_runs;     // newest last
RunRecord* g_recording = nullptr;
int g_serial = 0;

ReplayData* g_watchData = nullptr;  // handed to Gameplay (not owned by it in replay mode)
Watching g_watch;

// Testing
std::function<uint8_t(int)> g_testInput;
int g_observeStep = -1;
std::function<void(int)> g_observer;

// Overlay (child of the watching Gameplay's scene)
Node* g_overlay = nullptr;
Sprite* g_progress = nullptr;
Label* g_overlayState = nullptr;
float g_progressWidth = 0.0f;

Session* session() { return Settings::getInstance()->getCurrentSession(); }

size_t hashXml(const std::string& s) { return std::hash<std::string>()(s); }

void installCompletionListener() {
    static bool installed = false;
    if (installed) return;
    installed = true;
    // LevelB2D::levelCompleted dispatches "levelCompleted" to every listener; Gameplay's own
    // listener is scene-graph based, this one has a fixed priority and only takes notes.
    Director::getInstance()->getEventDispatcher()->addCustomEventListener("levelCompleted", [](EventCustom*) {
        if (!flashLevel() || !g_token.matches(session())) return;
        if (g_mode == Mode::Record && g_recording && !g_recording->completed) {
            g_recording->completed = true;
            g_recording->completeStep = g_steps;
        } else if (g_mode == Mode::Watch && !g_finished) {
            g_finished = true;
            if (g_overlayState) g_overlayState->setString("FINISHED IN " + formatTime((g_steps + stepsPerFlashFrame() - 1) / stepsPerFlashFrame()));
        }
    });
}

void buildOverlay() {
    Scene* scene = Director::getInstance()->getRunningScene();
    if (!scene) return;
    if (g_overlay) {
        g_overlay->removeFromParent();
        g_overlay->release();
    }
    const Size vs = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();
    g_overlay = Node::create();
    g_overlay->retain();
    g_overlay->setCascadeOpacityEnabled(true);
    const float w = std::min(1900.0f, vs.width - 1100.0f), h = 200.0f;
    auto* bg = ui::roundedRect(Size(w, h), 40.0f, Color3B(0, 0, 0), 150);
    bg->setAnchorPoint(Vec2::ZERO);
    g_overlay->addChild(bg);

    Label* tag = Label::createWithTTF("REPLAY", ui::kFontHeading, 58.0f);
    tag->setColor(ui::kPink);
    tag->setAnchorPoint(Vec2(0.0f, 0.5f));
    tag->setPosition(50.0f, h - 62.0f);
    g_overlay->addChild(tag);

    const ReplayInfo& r = g_watch.replay;
    std::string line = g_watch.title;
    line += "  \xC2\xB7  " + (r.completed() ? formatTime(r.frames) : std::string("did not finish"));
    if (r.character && r.character != playableCharacter(r.character))
        line += "  \xC2\xB7  " + ui::characterName(r.character) + " played as " +
                ui::characterName(playableCharacter(r.character));
    Label* who = Label::createWithTTF("", ui::kFontBodyBold, 46.0f);
    who->setColor(Color3B::WHITE);
    who->setAnchorPoint(Vec2(0.0f, 0.5f));
    who->setPosition(50.0f + tag->getContentSize().width + 34.0f, h - 60.0f);
    ui::setEllipsized(who, line, w - who->getPositionX() - 360.0f);
    g_overlay->addChild(who);

    g_overlayState = Label::createWithTTF("", ui::kFontHeading, 44.0f);
    g_overlayState->setColor(ui::kStarGold);
    g_overlayState->setAnchorPoint(Vec2(1.0f, 0.5f));
    g_overlayState->setPosition(w - 50.0f, h - 60.0f);
    g_overlay->addChild(g_overlayState);

    Label* note = Label::createWithTTF(
        browserPhysics()
            ? "Re-simulated from the recorded keys with the browser game's physics (Box2D 2.0 rules, 30 Hz). Very "
              "close, but not bit for bit: a long or chaotic run can still go differently."
            : "Approximate replay: re-simulated from the recorded keys. Browser physics is off (QOL), so it can go "
              "differently.",
        ui::kFontBody, 34.0f);
    note->setColor(ui::kTextDim);
    note->setAnchorPoint(Vec2(0.0f, 0.5f));
    note->setPosition(50.0f, 66.0f);
    ui::setEllipsized(note, note->getString(), w - 100.0f);
    g_overlay->addChild(note);

    g_progressWidth = w - 100.0f;
    auto* track = ui::roundedRect(Size(g_progressWidth, 14.0f), 7.0f, Color3B::WHITE, 60);
    track->setAnchorPoint(Vec2::ZERO);
    track->setPosition(50.0f, 24.0f);
    g_overlay->addChild(track);
    g_progress = Sprite::create();
    g_progress->setTextureRect(Rect(0, 0, 1.0f, 14.0f));
    g_progress->setColor(ui::kBlue);
    g_progress->setAnchorPoint(Vec2::ZERO);
    g_progress->setPosition(50.0f, 24.0f);
    g_overlay->addChild(g_progress);

    g_overlay->setContentSize(Size(w, h));
    // Top centre, below the game's timer.
    g_overlay->setPosition(origin.x + (vs.width - w) * 0.5f, origin.y + vs.height - h - 250.0f);
    scene->addChild(g_overlay, 9000);
}

void updateOverlay() {
    if (!g_overlay || !g_overlay->getParent()) return;
    const int total = std::max<int>(1, (int)g_watch.input.keys.size());
    const int frame = std::min(total, g_steps / stepsPerFlashFrame());
    g_progress->setScaleX(std::max(1.0f, g_progressWidth * frame / (float)total));
    if (!g_finished && frame >= total)
        g_overlayState->setString(g_watch.replay.completed() ? "END OF REPLAY" : "END (DID NOT FINISH)");
}

void startSession(ReplayData* data, bool isReplay) {
    g_token.bind(session());
    g_steps = 0;
    g_frameState = 0;
    g_mouseNext = 0;
    g_finished = false;
    g_mode = Mode::None;
    g_recording = nullptr;
    installCompletionListener();
    if (isReplay) {
        if (data && data == g_watchData) {
            g_mode = Mode::Watch;
            buildOverlay();
        }
        return;
    }
    if (!g_hasCurrent || hashXml(LevelSession::getInstance()->levelDataXML()) != g_currentHash) return;
    g_mode = Mode::Record;
    auto* run = new RunRecord();
    run->level = g_current;
    run->character = Settings::getInstance()->getSelectedCharacterId();
    run->serial = ++g_serial;
    run->stepsPerFrame = stepsPerFlashFrame();
    g_runs.push_back(run);
    g_recording = run;
    // Keep the last few runs (saved ones live on disk anyway).
    while (g_runs.size() > kKeptRuns) {
        auto victim = g_runs.begin();
        delete *victim;
        g_runs.erase(victim);
    }
}

Trigger* triggerAt(int index) {
    Session* s = session();
    LevelB2D* level = s ? s->getLevel() : nullptr;
    if (!level) return nullptr;
    const auto& triggers = level->onlineTriggers();
    return index >= 0 && index < (int)triggers.size() ? triggers[index] : nullptr;
}

int triggerIndex(Trigger* trigger) {
    Session* s = session();
    LevelB2D* level = s ? s->getLevel() : nullptr;
    if (!level || !trigger) return -1;
    const auto& triggers = level->onlineTriggers();
    auto it = std::find(triggers.begin(), triggers.end(), trigger);
    return it == triggers.end() ? -1 : (int)(it - triggers.begin());
}

void noteMouse(Trigger* trigger, bool rollOut) {
    if (g_mode != Mode::Record || !g_recording || !g_token.matches(session())) return;
    if (g_recording->completed || g_recording->tooLong) return;
    const int index = triggerIndex(trigger);
    if (index < 0) return;
    MouseEntry e;
    // Between frames: the click lands on the next Flash frame boundary.
    e.iteration = (g_steps + stepsPerFlashFrame() - 1) / stepsPerFlashFrame();
    e.triggerIndex = index;
    e.rollOut = rollOut;
    g_recording->mouse.push_back(e);
}

}  // namespace

// ---- RunRecord ---------------------------------------------------------------------------------

int RunRecord::frames() const {
    const int steps = completed ? completeStep : (int)this->steps.size();
    return std::min(kMaxReplayFrames, (steps + stepsPerFrame - 1) / stepsPerFrame);
}

ReplayInput RunRecord::toInput() const {
    ReplayInput in;
    const int n = frames();
    in.keys.reserve(n);
    for (int f = 0; f < n; ++f) {
        const size_t s = (size_t)f * stepsPerFrame;
        in.keys.push_back(mobileToFlash(s < steps.size() ? steps[s] : 0));
    }
    for (const MouseEntry& e : mouse)
        if (e.iteration < n) in.mouse.push_back(e);
    return in;
}

SavedRun RunRecord::toSavedRun() const {
    SavedRun run = saved;
    run.levelId = level.id;
    run.levelName = level.name;
    run.levelAuthorId = level.authorId;
    run.character = character;
    run.completed = completed;
    run.input = toInput();
    run.frames = (int)run.input.keys.size();
    return run;
}

// ---- hooks -------------------------------------------------------------------------------------

void gameplayState(ReplayData* data, bool isReplay, unsigned char* state) {
    if (!g_token.matches(session())) startSession(data, isReplay);
    if (g_mode == Mode::Watch) {
        // The byte of the Flash frame the next world step belongs to (one step per frame with
        // the browser physics profile, two on the mobile one).
        const size_t frame = (size_t)(g_steps / stepsPerFlashFrame());
        *state = frame < g_watch.input.keys.size() ? flashToMobile(g_watch.input.keys[frame]) : 0;
        updateOverlay();
    } else if (g_mode == Mode::Record) {
        if (g_testInput) *state = g_testInput(g_steps);
        g_frameState = *state;
    }
}

void physicsStep() {
    if (!g_token.matches(session())) return;
    if (g_observer && (g_steps == g_observeStep || g_observeStep == kObserveEveryStep) && g_mode != Mode::None) {
        auto observer = g_observer;
        observer(g_mode == Mode::Record ? 1 : 2);
    }
    if (g_mode == Mode::Watch) {
        // SessionReplay.run: the mouse entries of iteration i fire before frame i's input.
        if (g_steps % stepsPerFlashFrame() == 0) {
            const int iteration = g_steps / stepsPerFlashFrame();
            const auto& mouse = g_watch.input.mouse;
            while (g_mouseNext < mouse.size() && mouse[g_mouseNext].iteration < iteration) ++g_mouseNext;
            while (g_mouseNext < mouse.size() && mouse[g_mouseNext].iteration == iteration) {
                const MouseEntry& e = mouse[g_mouseNext++];
                if (Trigger* t = triggerAt(e.triggerIndex)) {
                    if (e.rollOut) t->onlineMouseMove(b2Vec2(-1.0e6f, -1.0e6f));
                    else t->onlineMouseClick();
                }
            }
        }
    } else if (g_mode == Mode::Record && g_recording && !g_recording->completed && !g_recording->tooLong) {
        if ((int)g_recording->steps.size() >= kMaxReplayFrames * g_recording->stepsPerFrame) {
            g_recording->tooLong = true;
        } else {
            g_recording->steps.push_back(g_frameState);
        }
    }
    ++g_steps;
}

void setTestInput(std::function<uint8_t(int)> input) { g_testInput = std::move(input); }

void setTestStepObserver(int step, std::function<void(int)> observer) {
    g_observeStep = step;
    g_observer = std::move(observer);
}

void noteClick(Trigger* trigger) { noteMouse(trigger, false); }
void noteRollOut(Trigger* trigger) { noteMouse(trigger, true); }

// ---- browser -----------------------------------------------------------------------------------

void beginOnlineRun(const OnlineLevelInfo& level) {
    g_current = level;
    g_currentHash = hashXml(LevelSession::getInstance()->levelDataXML());
    g_hasCurrent = true;
}

std::vector<RunRecord*> recentRuns(int levelId) {
    std::vector<RunRecord*> out;
    for (auto it = g_runs.rbegin(); it != g_runs.rend(); ++it)
        // Runs of at least half a second (15 Flash frames, at either physics profile).
        if ((levelId == 0 || (*it)->level.id == levelId) && (*it)->steps.size() >= (size_t)(15 * (*it)->stepsPerFrame))
            out.push_back(*it);
    return out;
}

RunRecord* runBySerial(int serial) {
    for (RunRecord* r : g_runs)
        if (r->serial == serial) return r;
    return nullptr;
}

int playableCharacter(int c) {
    switch (c) {
        case 1: case 2: case 3: case 4: case 5: case 9: return c;
        case 6: case 7: case 8: case 10: case 11: {
            // FlashLevelConverter's fallbacks.
            static const int kFallback[12] = {0, 0, 0, 0, 0, 0, 4, 4, 5, 0, 3, 2};
            return restored::hasCharacter(c) ? c : kFallback[c];
        }
        default: return 1;
    }
}

bool watch(const OnlineLevelInfo& level, const ReplayInfo& replay, const ReplayInput& input,
           const std::string& flashXml, const std::string& title, std::string* error) {
    ConversionReport report;
    const std::string mobile = FlashLevelConverter::toMobile(flashXml, &report);
    if (!report.ok) {
        if (error) *error = report.error.empty() ? "this level could not be converted" : report.error;
        return false;
    }
    const int character = playableCharacter(replay.character);
    LevelSession* ls = LevelSession::getInstance();
    ls->clearLevelData();
    ls->setChapterIndex(LevelStoreChapterImported);
    ls->setLevelDataXML(mobile);
    ls->setForceCharacter(true);
    ls->setCharacterIndex(character);
    ls->setVehicleIndex(0);
    ls->applyToSettings();
    // QOL (PC addition): the replay's character even where the level forces another one (a run
    // recorded with "any character", qol/CharacterChoice.h).
    qol::setCharacterOverride(character);

    if (!g_watchData) g_watchData = new ReplayData();
    g_watchData->reset();
    // The game's own per-frame entries (2 per Flash frame); the per-step choice in gameplayState
    // is what actually drives the character.
    for (uint8_t k : input.keys) {
        g_watchData->addEntry(flashToMobile(k));
        g_watchData->addEntry(flashToMobile(k));
    }
    g_watchData->resetPosition();
    g_watch.level = level;
    g_watch.replay = replay;
    g_watch.input = input;
    g_watch.title = title;
    // Not a run of the player.
    g_hasCurrent = false;
    Director::getInstance()->pushScene(Gameplay::createScene(mobile, g_watchData));
    return true;
}

}  // namespace replays
}  // namespace online
