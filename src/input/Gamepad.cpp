// PAD (PC addition): controller state core, see Gamepad.h.
#include "input/Gamepad.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <set>

#include "qol/QoL.h"

namespace openwheels {
namespace pad {
namespace {

// Press / release thresholds (hysteresis, so a stick resting near the threshold doesn't chatter).
const float kPressAt = 0.5f;
const float kReleaseAt = 0.35f;

std::function<void(std::vector<State>&)>& pollFn()
{
    static std::function<void(std::vector<State>&)> f;
    return f;
}

std::function<void(float, float)>& rumbleFn()
{
    static std::function<void(float, float)> f;
    return f;
}

struct Core
{
    std::vector<State> states;
    std::map<int, float> values;     // code -> largest value this frame
    std::vector<int> held;           // sorted
    std::vector<int> pressed;        // sorted
    std::vector<int> released;       // sorted
    std::vector<std::string> names;
    bool lastWasPad = false;

    std::function<void(int)> capture;
    std::set<int> captureIgnored;    // held when the capture started

    float clock = 0.0f;
    float rumbleEnd = -1.0f;         // clock time the playing rumble ends (< 0: none)
    float rumbleStrength = 0.0f;
};

Core& core()
{
    static Core c;
    return c;
}

bool contains(const std::vector<int>& sorted, int code)
{
    return std::binary_search(sorted.begin(), sorted.end(), code);
}

}  // namespace

void setPoll(std::function<void(std::vector<State>&)> poll) { pollFn() = std::move(poll); }
void setRumbleOutput(std::function<void(float, float)> rumble) { rumbleFn() = std::move(rumble); }

void update(float dt)
{
    Core& c = core();
    c.clock += dt;

    c.states.clear();
    if (pollFn()) pollFn()(c.states);

    c.names.clear();
    c.values.clear();
    for (const State& s : c.states)
    {
        c.names.push_back(s.name);
        for (int i = 0; i < kControlCount; i++)
        {
            if (s.control[i] > 0.0f) c.values[i] = std::max(c.values[i], s.control[i]);
        }
        for (const auto& r : s.raw)
        {
            if (r.second > 0.0f) c.values[r.first] = std::max(c.values[r.first], r.second);
        }
    }

    std::vector<int> held;
    for (const auto& v : c.values)
    {
        const bool was = contains(c.held, v.first);
        if (v.second >= kPressAt || (was && v.second >= kReleaseAt)) held.push_back(v.first);
    }
    c.pressed.clear();
    c.released.clear();
    std::set_difference(held.begin(), held.end(), c.held.begin(), c.held.end(), std::back_inserter(c.pressed));
    std::set_difference(c.held.begin(), c.held.end(), held.begin(), held.end(), std::back_inserter(c.released));
    c.held.swap(held);
    if (!c.pressed.empty()) c.lastWasPad = true;

    // Remap capture: the first new press (inputs held at the start must be let go first).
    for (int code : c.released) c.captureIgnored.erase(code);
    if (c.capture)
    {
        for (int code : c.pressed)
        {
            if (c.captureIgnored.count(code)) continue;
            std::function<void(int)> done;
            done.swap(c.capture);
            c.captureIgnored.clear();
            done(code);
            break;
        }
    }

    if (c.rumbleEnd >= 0.0f && c.clock >= c.rumbleEnd)
    {
        c.rumbleEnd = -1.0f;
        c.rumbleStrength = 0.0f;
        if (rumbleFn()) rumbleFn()(0.0f, 0.0f);
    }
}

std::vector<std::string> connectedNames() { return core().names; }
bool anyConnected() { return !core().states.empty(); }
bool lastInputWasPad() { return core().lastWasPad && anyConnected(); }
bool g_synthetic = false;
void noteOtherInput()
{
    if (!g_synthetic) core().lastWasPad = false;
}
void setSyntheticInput(bool on) { g_synthetic = on; }
bool syntheticInput() { return g_synthetic; }

bool held(int code) { return contains(core().held, code); }
bool pressed(int code) { return contains(core().pressed, code); }
bool released(int code) { return contains(core().released, code); }
const std::vector<int>& heldInputs() { return core().held; }

float value(int code)
{
    auto it = core().values.find(code);
    return it == core().values.end() ? 0.0f : it->second;
}

void leftStick(float* x, float* y)
{
    *x = value(kLRight) - value(kLLeft);
    *y = value(kLUp) - value(kLDown);
}

void rightStick(float* x, float* y)
{
    *x = value(kRRight) - value(kRLeft);
    *y = value(kRUp) - value(kRDown);
}

void beginCapture(std::function<void(int)> done)
{
    Core& c = core();
    c.capture = std::move(done);
    c.captureIgnored = std::set<int>(c.held.begin(), c.held.end());
}

void cancelCapture()
{
    std::function<void(int)> done;
    done.swap(core().capture);
    if (done) done(kNoInput);
}

bool capturing() { return (bool)core().capture; }

bool rumbleSupported() { return (bool)rumbleFn(); }
bool hapticsWanted() { return anyConnected() || qol::phoneVibration(); }

void rumble(float strength, float seconds)
{
    Core& c = core();
    strength *= qol::rumbleScale();
    if (!rumbleFn() || strength <= 0.01f || seconds <= 0.0f || !hapticsWanted()) return;
    strength = std::min(1.0f, strength);
    const bool playing = c.rumbleEnd >= 0.0f;
    if (playing && strength < c.rumbleStrength * 0.8f) return;  // a weaker hit under a strong one
    c.rumbleStrength = strength;
    c.rumbleEnd = std::max(playing ? c.rumbleEnd : 0.0f, c.clock + seconds);
    rumbleFn()(strength, c.rumbleEnd - c.clock);
}

std::string inputName(int code)
{
    static const char* const kNames[kControlCount] = {
        "a", "b", "x", "y", "lb", "rb", "lt", "rt", "back", "start", "guide", "l3", "r3",
        "d-pad up", "d-pad down", "d-pad left", "d-pad right",
        "l-stick up", "l-stick down", "l-stick left", "l-stick right",
        "r-stick up", "r-stick down", "r-stick left", "r-stick right"};
    if (code >= 0 && code < kControlCount) return kNames[code];
    if (code >= kRawButton && code < kRawButton + kRawLimit) return "button " + std::to_string(code - kRawButton);
    if (code >= kRawAxisPlus && code < kRawAxisPlus + kRawLimit) return "axis " + std::to_string(code - kRawAxisPlus) + "+";
    if (code >= kRawAxisMinus && code < kRawAxisMinus + kRawLimit) return "axis " + std::to_string(code - kRawAxisMinus) + "-";
    return "-";
}

}  // namespace pad
}  // namespace openwheels
