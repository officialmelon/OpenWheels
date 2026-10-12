// PAD (PC addition): game controllers, rumble and tilt on Android (see src/input/Gamepad.h and
// src/platform/android/java/org/openwheels/game/GameInput.java).
//
// Android maps every controller it knows onto the standard gamepad key codes and axes (the
// Xbox-style layout), so all of them - Bluetooth / USB pads and the built-in controls of handhelds
// such as the AYN Odin / Thor - report the standard controls. The d-pad comes as key codes or as
// the hat axes, the triggers as L2 / R2 keys and / or the LTRIGGER / RTRIGGER (or BRAKE / GAS)
// axes: both are merged. Other buttons (C, Z, BUTTON_1..16) are raw codes, bindable on the QoL
// controller page.
//
// GameInput calls the JNI functions below on the UI thread; the GL thread polls a copy.
#include <jni.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include "cocos2d.h"
#include "platform/android/jni/JniHelper.h"

#include "input/Gamepad.h"
#include "input/Tilt.h"
#include "qol/QoL.h"

namespace openwheels {
namespace pad {
namespace {

// GameInput.AXES order.
enum Axis { kAxisX, kAxisY, kAxisZ, kAxisRZ, kAxisRX, kAxisRY, kHatX, kHatY, kLTrigger, kRTrigger, kBrake, kGas, kAxisCount };

// android.view.KeyEvent key codes.
enum KeyCode
{
    kKeyDpadUp = 19,
    kKeyDpadDown = 20,
    kKeyDpadLeft = 21,
    kKeyDpadRight = 22,
    kKeyDpadCenter = 23,
    kKeyA = 96,
    kKeyB = 97,
    kKeyC = 98,
    kKeyX = 99,
    kKeyY = 100,
    kKeyZ = 101,
    kKeyL1 = 102,
    kKeyR1 = 103,
    kKeyL2 = 104,
    kKeyR2 = 105,
    kKeyThumbL = 106,
    kKeyThumbR = 107,
    kKeyStart = 108,
    kKeySelect = 109,
    kKeyMode = 110,
};

struct Device
{
    std::string name;
    std::set<int> keys;
    float axes[kAxisCount] = {};
};

std::mutex& lock()
{
    static std::mutex m;
    return m;
}

std::map<int, Device>& devices()
{
    static std::map<int, Device> d;
    return d;
}

float positive(float v) { return v > 0.0f ? std::min(1.0f, v) : 0.0f; }
float negative(float v) { return v < 0.0f ? std::min(1.0f, -v) : 0.0f; }

void fill(State& s, const Device& d)
{
    auto key = [&d](int code) { return d.keys.count(code) ? 1.0f : 0.0f; };
    const float* a = d.axes;
    float* c = s.control;
    c[kA] = std::max(key(kKeyA), key(kKeyDpadCenter));
    c[kB] = key(kKeyB);
    c[kX] = key(kKeyX);
    c[kY] = key(kKeyY);
    c[kLB] = key(kKeyL1);
    c[kRB] = key(kKeyR1);
    c[kLT] = std::max({key(kKeyL2), positive(a[kLTrigger]), positive(a[kBrake])});
    c[kRT] = std::max({key(kKeyR2), positive(a[kRTrigger]), positive(a[kGas])});
    c[kBack] = key(kKeySelect);
    c[kStart] = key(kKeyStart);
    c[kGuide] = key(kKeyMode);
    c[kLS] = key(kKeyThumbL);
    c[kRS] = key(kKeyThumbR);
    c[kDUp] = std::max(key(kKeyDpadUp), negative(a[kHatY]));
    c[kDDown] = std::max(key(kKeyDpadDown), positive(a[kHatY]));
    c[kDLeft] = std::max(key(kKeyDpadLeft), negative(a[kHatX]));
    c[kDRight] = std::max(key(kKeyDpadRight), positive(a[kHatX]));
    // Sticks: y down = positive.
    c[kLUp] = negative(a[kAxisY]);
    c[kLDown] = positive(a[kAxisY]);
    c[kLLeft] = negative(a[kAxisX]);
    c[kLRight] = positive(a[kAxisX]);
    c[kRUp] = negative(a[kAxisRZ]);
    c[kRDown] = positive(a[kAxisRZ]);
    c[kRLeft] = negative(a[kAxisZ]);
    c[kRRight] = positive(a[kAxisZ]);
    // Everything else as raw codes: key codes, and RX / RY (some pads' triggers or second stick).
    for (int code : d.keys)
    {
        switch (code)
        {
        case kKeyDpadUp: case kKeyDpadDown: case kKeyDpadLeft: case kKeyDpadRight: case kKeyDpadCenter:
        case kKeyA: case kKeyB: case kKeyX: case kKeyY: case kKeyL1: case kKeyR1: case kKeyL2: case kKeyR2:
        case kKeyThumbL: case kKeyThumbR: case kKeyStart: case kKeySelect: case kKeyMode:
            break;
        default:
            if (code >= 0 && code < kRawLimit) s.raw.push_back({kRawButton + code, 1.0f});
            break;
        }
    }
    s.raw.push_back({kRawAxisPlus + kAxisRX, positive(a[kAxisRX])});
    s.raw.push_back({kRawAxisMinus + kAxisRX, negative(a[kAxisRX])});
    s.raw.push_back({kRawAxisPlus + kAxisRY, positive(a[kAxisRY])});
    s.raw.push_back({kRawAxisMinus + kAxisRY, negative(a[kAxisRY])});
}

void poll(std::vector<State>& out)
{
    std::lock_guard<std::mutex> guard(lock());
    for (const auto& entry : devices())
    {
        State s;
        s.id = entry.first;
        s.name = entry.second.name;
        fill(s, entry.second);
        out.push_back(s);
    }
}

void platformRumble(float strength, float seconds)
{
    cocos2d::JniHelper::callStaticVoidMethod("org/openwheels/game/AppActivity", "padRumble", strength,
                                             (int)(seconds * 1000.0f + 0.5f), qol::phoneVibration());
}

}  // namespace

void installPlatformProvider()
{
    setPoll(poll);
    setRumbleOutput(platformRumble);
}

}  // namespace pad

namespace tilt {

void enableSensor(bool on)
{
    cocos2d::JniHelper::callStaticVoidMethod("org/openwheels/game/AppActivity", "padTiltSensor", on);
}

}  // namespace tilt
}  // namespace openwheels

using openwheels::pad::devices;
using openwheels::pad::lock;

extern "C" {

JNIEXPORT void JNICALL Java_org_openwheels_game_GameInput_nativeGamepadConnected(JNIEnv* env, jclass, jint id, jstring name)
{
    const char* chars = name ? env->GetStringUTFChars(name, nullptr) : nullptr;
    std::lock_guard<std::mutex> guard(lock());
    devices()[id].name = chars ? chars : "controller";
    if (chars) env->ReleaseStringUTFChars(name, chars);
}

JNIEXPORT void JNICALL Java_org_openwheels_game_GameInput_nativeGamepadRemoved(JNIEnv*, jclass, jint id)
{
    std::lock_guard<std::mutex> guard(lock());
    devices().erase(id);
}

JNIEXPORT void JNICALL Java_org_openwheels_game_GameInput_nativeGamepadKey(JNIEnv*, jclass, jint id, jint keyCode, jboolean down)
{
    std::lock_guard<std::mutex> guard(lock());
    auto& d = devices()[id];
    if (d.name.empty()) d.name = "controller";
    if (down) d.keys.insert(keyCode);
    else d.keys.erase(keyCode);
}

JNIEXPORT void JNICALL Java_org_openwheels_game_GameInput_nativeGamepadAxes(JNIEnv* env, jclass, jint id, jfloatArray values)
{
    const jsize n = std::min<jsize>(env->GetArrayLength(values), openwheels::pad::kAxisCount);
    std::lock_guard<std::mutex> guard(lock());
    auto& d = devices()[id];
    if (d.name.empty()) d.name = "controller";
    env->GetFloatArrayRegion(values, 0, n, d.axes);
}

JNIEXPORT void JNICALL Java_org_openwheels_game_GameInput_nativeTilt(JNIEnv*, jclass, jfloat roll)
{
    openwheels::tilt::setRoll(roll);
}

}  // extern "C"
