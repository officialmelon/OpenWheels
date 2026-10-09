// PAD (PC addition): controller rumble from the game's sounds, see Haptics.h.
#include "input/Haptics.h"

#include <algorithm>

#include "input/Gamepad.h"

namespace openwheels {
namespace haptics {
namespace {

bool g_gameplay = false;

struct Rule
{
    const char* part;  // substring of the sound name
    float strength;
    float seconds;
};

// First match wins. The names are the game's sound list (e.g. "MineExplosion", "NeckBreak",
// "LimbRip2", "BikeSmash1", "TireHit1", "GlassImpact").
const Rule kRules[] = {
    {"Explosion", 1.0f, 0.45f},
    {"Explode", 1.0f, 0.45f},
    {"Meteor", 0.9f, 0.35f},
    {"NeckBreak", 0.85f, 0.25f},
    {"HeadSmash", 0.85f, 0.25f},
    {"ChestSmash", 0.75f, 0.22f},
    {"PelvisSmash", 0.75f, 0.22f},
    {"BoneBreak", 0.7f, 0.2f},
    {"LimbRip", 0.7f, 0.2f},
    {"Gore", 0.6f, 0.18f},
    {"Smash", 0.55f, 0.16f},
    {"Break", 0.5f, 0.15f},
    {"Crush", 0.5f, 0.15f},
    {"Splat", 0.4f, 0.12f},
    {"Impact", 0.35f, 0.1f},
    {"Crash", 0.35f, 0.1f},
    {"Hit", 0.25f, 0.07f},
    {"Thud", 0.25f, 0.07f},
    {"Land", 0.25f, 0.07f},
    {"Fire", 0.3f, 0.08f},  // harpoon / arrow guns
    {"HeartBeat", 0.2f, 0.07f},
    {"Victory", 0.45f, 0.3f},
};

}  // namespace

void setGameplayActive(bool active) { g_gameplay = active; }

void onSound(const std::string& name, float distance)
{
    if (!g_gameplay || !pad::hapticsWanted() || !pad::rumbleSupported()) return;
    for (const Rule& rule : kRules)
    {
        if (name.find(rule.part) == std::string::npos) continue;
        // Full strength within 8 m of the camera, nothing past 35 m.
        const float fade = 1.0f - std::max(0.0f, std::min(1.0f, (distance - 8.0f) / 27.0f));
        if (fade > 0.05f) pad::rumble(rule.strength * fade, rule.seconds);
        return;
    }
}

}  // namespace haptics
}  // namespace openwheels
