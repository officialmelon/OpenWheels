#pragma once
// ONLINE (PC addition): runtime support for browser (Flash/HTML5) levels converted by
// FlashLevelConverter. Everything here is only active while a converted level is loaded: the
// converter marks its output with <info src="flash" fv="<browser version>">, LevelB2D::addInfo
// reports that through setFlashLevel() for every level it loads (campaign / editor levels
// switch it off again), and every game-code hook that changes behaviour checks flashLevel().
//
// Art: Flash library symbols rendered from the player's own browser-game SWF at build time by
// tools/assets/extract_flash_items.py into <exe dir>/generated/flash/<name>.png, with
// generated/flash/index.tsv listing name, registration point and render zoom. Never committed.

#include <string>
#include <vector>

#include "Box2D/Box2D.h"
#include "math/Vec2.h"

namespace cocos2d {
class Node;
class Sprite;
}

class LevelB2D;
class Session;
class Trigger;

namespace online {

// --- gate -------------------------------------------------------------------------------------

bool flashLevel();
// Browser version of the level (info "fv", = "v" of the browser XML); 0 when not a flash level.
float flashVersion();
void setFlashLevel(bool on, float browserVersion);

// --- units ------------------------------------------------------------------------------------
// Flash editor pixels (y down, 62.5 px/m) <-> Box2D metres of the mobile world (y up, stage
// flipped at sh = 10000 px). Valid for converted levels only (ptm 62.5, sw 20000, sh 10000, r 1).
const float kFlashPtm = 62.5f;
const float kFlashStageHeightPx = 10000.0f;

inline b2Vec2 flashToWorld(float xPx, float yPx)
{
    return b2Vec2(xPx / kFlashPtm, (kFlashStageHeightPx - yPx) / kFlashPtm);
}
inline b2Vec2 worldToFlash(const b2Vec2& m)
{
    return b2Vec2(m.x * kFlashPtm, kFlashStageHeightPx - m.y * kFlashPtm);
}
// A Flash direction / offset (y down) as a world vector (y up), both in metres or both in px.
inline b2Vec2 flashDir(float x, float y) { return b2Vec2(x, -y); }
// Flash rotation (degrees, clockwise) -> Box2D angle (radians, counter-clockwise).
inline float flashAngle(float degrees) { return degrees * -0.017453292f; }

// Display points per Flash pixel in the gameplay session (session ptm / 62.5; 4 on every tier).
float pointsPerFlashPx();

// --- art --------------------------------------------------------------------------------------

// True when generated/flash/<name>.png exists (the SWF art was extracted at build time).
bool hasFlashArt(const std::string& name);
// A sprite of a rendered Flash symbol frame: anchored on the symbol's registration point and
// scaled so one Flash pixel covers pointsPerFlashPx() points. nullptr when the art is missing.
cocos2d::Sprite* createFlashSprite(const std::string& name);

// --- layers -----------------------------------------------------------------------------------
// Flash adds most user-level display objects to level.background (above the static shapes,
// below the characters) or level.foreground. These are session children created on demand at
// z 1 (after the shape layer) and z 14 (after the foreground shapes).
cocos2d::Node* flashBackgroundLayer();
cocos2d::Node* flashForegroundLayer();

// --- physics ----------------------------------------------------------------------------------
// Called before every world step of a converted level. The browser game runs Box2D 2.0, whose
// polygons have no skin: shapes touch only when they overlap. Box2D 2.3 rounds every polygon by
// b2_polygonRadius (0.01 m, 0.6 Flash px), so items the browser level placed a pixel apart
// already touch (e.g. POKEMON TRAINING's mine row under a resting bar exploded at the start).
// Polygon fixtures of converted levels get radius 0 (new fixtures are picked up every step).
//
// Box2D 2.3 also wakes both bodies whenever their fattened bounding boxes start to overlap
// (b2ContactManager::AddPair), up to ~0.2 m before they touch; Box2D 2.0 only wakes bodies that
// really collide. Sleeping bodies of a browser level (p6 "sleeping" shapes/groups/items) woke on
// the first step that way next to any wall. flashPostStep puts every body that was asleep before
// the step and got woken without a touching contact (and without a joint to a body that was
// awake) back to sleep where it was.
void flashPreStep(b2World* world);
void flashPostStep(b2World* world);

// A non-fixed shape of density NaN is static in Box2D 2.0 (mass NaN, so no inverse mass), but its
// centre of mass is NaN too, and the contact solver multiplies that NaN into the impulses: every
// dynamic body that touches it, and every body joined to those through joints or contacts in the
// same island, gets NaN velocities and positions and is frozen by Box2D 2.0 (out of the world's
// bounds). Levels use it on purpose: CLICK PARKOUR 3 (10254164) poisons its "hide vehicle"
// character on the first frames so that Flash's camera, fed a NaN focus, snaps to the top-left
// corner of the stage where the level's click buttons are. flashPostStep emulates it without
// putting NaNs into Box2D 2.3: those bodies are stopped and deactivated, and flashNanBody tells
// StageCamera to treat its focus like Flash's NaN one.
bool flashNanBody(b2Body* body);

// --- click triggers (triggered by "mouse click", b = 6) ---------------------------------------
// Installs the touch/mouse listener for the current gameplay session (idempotent).
void installClickTriggers(LevelB2D* level);

// Identifies the live Session. Comparing Session pointers is not enough: a restarted level's
// new Session is often allocated at the address the previous one was freed from, so state keyed by
// the pointer would keep using the dead session's nodes. bind() parents an invisible marker node to
// the session; a destroyed session clears its children's parent pointers, so matches() turns false.
class SessionToken {
public:
    bool matches(Session* session) const;
    void bind(Session* session);
    void reset();

private:
    cocos2d::Node* _marker = nullptr;  // retained
};

}  // namespace online
