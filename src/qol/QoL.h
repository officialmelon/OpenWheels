#pragma once
// Quality-of-life options (not part of the 1:1 reconstruction). Every option defaults to the
// original game's behaviour; game code consults these at points marked "QOL (PC addition):".
// Persisted in UserDefault under "qol_*".

#include <functional>

namespace cocos2d {
class Node;
}

namespace qol {

// Blood rendering, after the browser game's four blood settings (Flash v1.60+):
//   Classic   - the mobile game's particles (original)
//   Streaks   - particles drawn as short streaks along their velocity (browser setting 2)
//   Liquid    - particles merged into flowing liquid blobs (browser setting 3)
//   Realistic - liquid with a glossy, lit surface (browser setting 4)
enum class BloodStyle { Classic = 0, Streaks = 1, Liquid = 2, Realistic = 3 };

BloodStyle bloodStyle();
void setBloodStyle(BloodStyle style);
const char* bloodStyleName(BloodStyle style);

// Session particle budget (original: 2000, Session::canAddEmitter).
int maxParticles();
void setMaxParticles(int count);

// Gameplay camera zoom: 1 = original; < 1 shows more of the level.
float cameraZoom();
void setCameraZoom(float zoom);

bool showFps();
void setShowFps(bool on);

// Treat every campaign level as unlocked (level select).
bool unlockAllLevels();
void setUnlockAllLevels(bool on);

// Fullscreen (desktop builds only; the platform layer installs the handler).
bool fullscreenSupported();
bool fullscreen();
void setFullscreen(bool on);
void setFullscreenHandler(std::function<void(bool)> handler);

// Applies display options (FPS counter, fullscreen) - call once the GL view exists.
void applyDisplaySettings();

// Blood emitters are tagged at creation so renderers can single them out.
void markBlood(cocos2d::Node* emitter);
bool isBlood(const cocos2d::Node* emitter);

}  // namespace qol
