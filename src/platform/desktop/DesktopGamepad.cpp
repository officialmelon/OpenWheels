// PAD (PC addition): game controllers on Windows, Linux and macOS (see src/input/Gamepad.h).
//
// GLFW 3.2 (the engine's prebuilt) reports joysticks as plain axes and buttons in the order the
// OS driver gives them, without the gamepad mappings of later GLFW versions, so the layouts of
// the common controllers are mapped here:
//   * Windows, XInput pads (Xbox 360 / One / Series and the many pads that emulate them): GLFW's
//     XInput order, Y axes up = positive, triggers -1..1;
//   * Windows / Linux, PlayStation pads (DualShock 4, DualSense): DirectInput resp. hid-playstation
//     order;
//   * Linux, xpad-style pads (Xbox pads, Steam Input's virtual pad, most X-input mode pads):
//     joydev order, hats as axes 6 / 7 (or buttons 11-14 with xpad's dpad_to_buttons);
//   * anything else (and every controller on macOS): the left stick from axes 0 / 1, the rest as
//     raw buttons / axes, bindable on the QoL controller page.
// Rumble: Windows through XInput (XInput pads), Linux through the force feedback interface of
// the controller's event device (needs write access, which desktop distributions give the
// logged-in user for game controllers). macOS: none. No motion sensor on desktop.
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "cocos2d.h"

#include "input/Gamepad.h"
#include "input/Tilt.h"

#if CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#endif

namespace openwheels {
namespace pad {
namespace {

enum class Layout { Generic, XInput, PlayStationDInput, Xpad, PlayStationLinux };

std::string lower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

Layout layoutFor(const std::string& name, int axes, int buttons)
{
    const std::string n = lower(name);
    const bool playStation = has(n, "wireless controller") || has(n, "dualsense") || has(n, "dualshock") ||
                             has(n, "playstation") || has(n, "ps4") || has(n, "ps5") || has(n, "sony");
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
    if ((has(n, "xinput") || has(n, "xbox")) && axes >= 6 && buttons >= 14) return Layout::XInput;
    if (playStation && axes >= 6 && buttons >= 18) return Layout::PlayStationDInput;
#elif CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
    if (playStation && axes >= 6 && buttons >= 13) return Layout::PlayStationLinux;
    const bool xbox = has(n, "x-box") || has(n, "xbox") || has(n, "xinput") || has(n, "microsoft");
    if ((xbox && axes >= 6 && buttons >= 11) || (axes == 8 && buttons == 11)) return Layout::Xpad;
#endif
    (void)playStation;
    (void)axes;
    (void)buttons;
    return Layout::Generic;
}

float positive(float v) { return v > 0.0f ? std::min(1.0f, v) : 0.0f; }
float negative(float v) { return v < 0.0f ? std::min(1.0f, -v) : 0.0f; }
float trigger(float v) { return std::max(0.0f, std::min(1.0f, (v + 1.0f) * 0.5f)); }  // rests at -1

// x right, y down = positive.
void stick(State& s, float x, float y, int up)
{
    s.control[up] = negative(y);       // up
    s.control[up + 1] = positive(y);   // down
    s.control[up + 2] = negative(x);   // left
    s.control[up + 3] = positive(x);   // right
}

struct Raw
{
    const float* axes;
    int axisCount;
    const unsigned char* buttons;
    int buttonCount;
    float axis(int i) const { return i < axisCount ? axes[i] : 0.0f; }
    float button(int i) const { return i < buttonCount && buttons[i] == GLFW_PRESS ? 1.0f : 0.0f; }
};

void fill(State& s, Layout layout, const Raw& r)
{
    float* c = s.control;
    switch (layout)
    {
    case Layout::XInput:
        c[kA] = r.button(0); c[kB] = r.button(1); c[kX] = r.button(2); c[kY] = r.button(3);
        c[kLB] = r.button(4); c[kRB] = r.button(5); c[kBack] = r.button(6); c[kStart] = r.button(7);
        c[kLS] = r.button(8); c[kRS] = r.button(9);
        c[kDUp] = r.button(10); c[kDRight] = r.button(11); c[kDDown] = r.button(12); c[kDLeft] = r.button(13);
        stick(s, r.axis(0), -r.axis(1), kLUp);
        stick(s, r.axis(2), -r.axis(3), kRUp);
        c[kLT] = trigger(r.axis(4)); c[kRT] = trigger(r.axis(5));
        break;
    case Layout::PlayStationDInput:
    {
        // square, cross, circle, triangle, L1, R1, L2, R2, share, options, L3, R3, PS, touchpad
        // (DualSense: + mute), then the hat as up / right / down / left.
        c[kX] = r.button(0); c[kA] = r.button(1); c[kB] = r.button(2); c[kY] = r.button(3);
        c[kLB] = r.button(4); c[kRB] = r.button(5); c[kBack] = r.button(8); c[kStart] = r.button(9);
        c[kLS] = r.button(10); c[kRS] = r.button(11); c[kGuide] = r.button(12);
        const int hat = r.buttonCount - 4;
        c[kDUp] = r.button(hat); c[kDRight] = r.button(hat + 1); c[kDDown] = r.button(hat + 2); c[kDLeft] = r.button(hat + 3);
        stick(s, r.axis(0), r.axis(1), kLUp);
        stick(s, r.axis(2), r.axis(5), kRUp);
        c[kLT] = std::max(trigger(r.axis(3)), r.button(6));
        c[kRT] = std::max(trigger(r.axis(4)), r.button(7));
        if (r.buttonCount > 13) s.raw.push_back({kRawButton + 13, r.button(13)});  // touchpad click
        break;
    }
    case Layout::Xpad:
    case Layout::PlayStationLinux:
    {
        if (layout == Layout::Xpad)
        {
            c[kA] = r.button(0); c[kB] = r.button(1); c[kX] = r.button(2); c[kY] = r.button(3);
            c[kLB] = r.button(4); c[kRB] = r.button(5); c[kBack] = r.button(6); c[kStart] = r.button(7);
            c[kGuide] = r.button(8); c[kLS] = r.button(9); c[kRS] = r.button(10);
            if (r.buttonCount >= 15)  // xpad dpad_to_buttons: left, right, up, down
            {
                c[kDLeft] = r.button(11); c[kDRight] = r.button(12); c[kDUp] = r.button(13); c[kDDown] = r.button(14);
            }
        }
        else
        {
            // cross, circle, triangle (BTN_NORTH), square (BTN_WEST), L1, R1, L2, R2, share,
            // options, PS, L3, R3.
            c[kA] = r.button(0); c[kB] = r.button(1); c[kY] = r.button(2); c[kX] = r.button(3);
            c[kLB] = r.button(4); c[kRB] = r.button(5); c[kBack] = r.button(8); c[kStart] = r.button(9);
            c[kGuide] = r.button(10); c[kLS] = r.button(11); c[kRS] = r.button(12);
        }
        stick(s, r.axis(0), r.axis(1), kLUp);
        stick(s, r.axis(3), r.axis(4), kRUp);
        c[kLT] = trigger(r.axis(2));
        c[kRT] = trigger(r.axis(5));
        if (layout == Layout::PlayStationLinux)
        {
            c[kLT] = std::max(c[kLT], r.button(6));
            c[kRT] = std::max(c[kRT], r.button(7));
        }
        if (r.axisCount >= 8)
        {
            c[kDLeft] = std::max(c[kDLeft], negative(r.axis(6)));
            c[kDRight] = std::max(c[kDRight], positive(r.axis(6)));
            c[kDUp] = std::max(c[kDUp], negative(r.axis(7)));
            c[kDDown] = std::max(c[kDDown], positive(r.axis(7)));
        }
        break;
    }
    case Layout::Generic:
        if (r.axisCount >= 2) stick(s, r.axis(0), r.axis(1), kLUp);
        for (int i = 2; i < r.axisCount && i < kRawLimit; i++)
        {
            s.raw.push_back({kRawAxisPlus + i, positive(r.axis(i))});
            s.raw.push_back({kRawAxisMinus + i, negative(r.axis(i))});
        }
        for (int i = 0; i < r.buttonCount && i < kRawLimit; i++) s.raw.push_back({kRawButton + i, r.button(i)});
        break;
    }
}

int g_connected = 0;

void poll(std::vector<State>& out)
{
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; j++)
    {
        if (!glfwJoystickPresent(j)) continue;
        Raw r;
        r.axes = glfwGetJoystickAxes(j, &r.axisCount);
        r.buttons = glfwGetJoystickButtons(j, &r.buttonCount);
        if (!r.axes) r.axisCount = 0;
        if (!r.buttons) r.buttonCount = 0;
        State s;
        s.id = j;
        const char* name = glfwGetJoystickName(j);
        s.name = name ? name : "controller";
        fill(s, layoutFor(s.name, r.axisCount, r.buttonCount), r);
        out.push_back(s);
    }
    g_connected = (int)out.size();
}

// ---- rumble --------------------------------------------------------------------------------------

#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32

struct XInputVibration
{
    WORD left;   // low-frequency motor
    WORD right;  // high-frequency motor
};
typedef DWORD(WINAPI* XInputSetStateFn)(DWORD, XInputVibration*);

XInputSetStateFn xinputSetState()
{
    static bool loaded = false;
    static XInputSetStateFn fn = nullptr;
    if (!loaded)
    {
        loaded = true;
        const char* const dlls[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
        for (const char* dll : dlls)
        {
            if (HMODULE module = LoadLibraryA(dll))
            {
                fn = (XInputSetStateFn)GetProcAddress(module, "XInputSetState");
                if (fn) break;
            }
        }
    }
    return fn;
}

void platformRumble(float strength, float)
{
    XInputSetStateFn setState = xinputSetState();
    if (!setState) return;
    XInputVibration v;
    v.left = (WORD)(strength * 65535.0f);
    v.right = (WORD)(strength * 0.7f * 65535.0f);
    for (DWORD i = 0; i < 4; i++) setState(i, &v);  // not connected: ERROR_DEVICE_NOT_CONNECTED
}

#elif CC_TARGET_PLATFORM == CC_PLATFORM_LINUX

struct ForceFeedback
{
    int fd = -1;
    int effect = -1;
};

std::vector<ForceFeedback>& ffDevices()
{
    static std::vector<ForceFeedback> devices;
    return devices;
}

int g_ffScannedFor = -1;  // controller count of the last scan

// The event devices of the joysticks (/sys/class/input/jsN/device/eventM) that can rumble.
void scanForceFeedback()
{
    for (ForceFeedback& d : ffDevices()) close(d.fd);
    ffDevices().clear();
    DIR* input = opendir("/sys/class/input");
    if (!input) return;
    while (dirent* js = readdir(input))
    {
        if (std::strncmp(js->d_name, "js", 2) != 0) continue;
        const std::string deviceDir = std::string("/sys/class/input/") + js->d_name + "/device";
        DIR* device = opendir(deviceDir.c_str());
        if (!device) continue;
        while (dirent* e = readdir(device))
        {
            if (std::strncmp(e->d_name, "event", 5) != 0) continue;
            const std::string path = std::string("/dev/input/") + e->d_name;
            const int fd = open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) continue;
            unsigned long features[(FF_MAX + 8 * sizeof(unsigned long)) / (8 * sizeof(unsigned long))] = {};
            const bool rumble = ioctl(fd, EVIOCGBIT(EV_FF, sizeof(features)), features) >= 0 &&
                                (features[FF_RUMBLE / (8 * sizeof(unsigned long))] >> (FF_RUMBLE % (8 * sizeof(unsigned long)))) & 1;
            if (!rumble)
            {
                close(fd);
                continue;
            }
            ForceFeedback d;
            d.fd = fd;
            ffDevices().push_back(d);
        }
        closedir(device);
    }
    closedir(input);
}

void platformRumble(float strength, float seconds)
{
    if (g_ffScannedFor != g_connected)
    {
        g_ffScannedFor = g_connected;
        scanForceFeedback();
    }
    for (ForceFeedback& d : ffDevices())
    {
        if (strength > 0.0f)
        {
            ff_effect effect;
            std::memset(&effect, 0, sizeof(effect));
            effect.type = FF_RUMBLE;
            effect.id = (short)d.effect;
            effect.u.rumble.strong_magnitude = (unsigned short)(strength * 65535.0f);
            effect.u.rumble.weak_magnitude = (unsigned short)(strength * 0.7f * 65535.0f);
            effect.replay.length = (unsigned short)std::min(5000.0f, seconds * 1000.0f + 20.0f);
            if (ioctl(d.fd, EVIOCSFF, &effect) < 0)
            {
                d.effect = -1;
                continue;
            }
            d.effect = effect.id;
        }
        if (d.effect < 0) continue;
        input_event play;
        std::memset(&play, 0, sizeof(play));
        play.type = EV_FF;
        play.code = (unsigned short)d.effect;
        play.value = strength > 0.0f ? 1 : 0;
        if (write(d.fd, &play, sizeof(play)) < 0) g_ffScannedFor = -1;  // unplugged: scan again next time
    }
}

#endif

}  // namespace

void installPlatformProvider()
{
    setPoll(poll);
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32 || CC_TARGET_PLATFORM == CC_PLATFORM_LINUX
    setRumbleOutput(platformRumble);
#endif
}

}  // namespace pad

namespace tilt {
void enableSensor(bool) {}  // no motion sensor on desktop builds
}  // namespace tilt

}  // namespace openwheels
