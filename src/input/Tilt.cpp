// PAD (PC addition): tilt steering, see Tilt.h.
#include "input/Tilt.h"

#include <atomic>
#include <cmath>

#include "qol/QoL.h"

namespace openwheels {
namespace tilt {
namespace {

std::atomic<float> g_roll(0.0f);
std::atomic<bool> g_fresh(false);  // a reading arrived since the sensor started
bool g_sensorOn = false;
int g_lean = 0;  // -1 lean forward, +1 lean back

float thresholdDegrees(qol::TiltSteering mode)
{
    switch (mode)
    {
    case qol::TiltSteering::Low: return 20.0f;
    case qol::TiltSteering::Medium: return 12.0f;
    case qol::TiltSteering::High: return 7.0f;
    default: return 1000.0f;
    }
}

}  // namespace

void setRoll(float radians)
{
    g_roll = radians;
    g_fresh = true;
}

void update(bool gameplayActive)
{
    const qol::TiltSteering mode = qol::tiltSteeringSupported() ? qol::tiltSteering() : qol::TiltSteering::Off;
    const bool want = gameplayActive && mode != qol::TiltSteering::Off;
    if (want != g_sensorOn)
    {
        g_sensorOn = want;
        g_fresh = false;
        enableSensor(want);
    }
    if (!want || !g_fresh)
    {
        g_lean = 0;
        return;
    }
    const float degrees = g_roll * 57.29578f;
    const float on = thresholdDegrees(mode), off = on * 0.7f;  // let go a little before the threshold
    if (g_lean == 0)
    {
        if (degrees > on) g_lean = 1;
        else if (degrees < -on) g_lean = -1;
    }
    else if (g_lean == 1 && degrees < off)
    {
        g_lean = degrees < -on ? -1 : 0;
    }
    else if (g_lean == -1 && degrees > -off)
    {
        g_lean = degrees > on ? 1 : 0;
    }
}

bool leanBack() { return g_lean == 1; }
bool leanForward() { return g_lean == -1; }

}  // namespace tilt
}  // namespace openwheels
