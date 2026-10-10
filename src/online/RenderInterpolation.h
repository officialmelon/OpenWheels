#pragma once
// ONLINE (PC addition): smooth drawing of the browser physics profile.
//
// With "browser physics" (online/FlashPhysics.h) a browser level steps its world once per 30 Hz
// Flash frame, so drawing the bodies where the last step left them moves everything on screen at
// 30 Hz while the game draws at 60 (or more). Session::update instead draws every display frame
// between the last two steps: each body's transform is interpolated from where it was before the
// last step to where it is now (alpha = time accumulated since that step / the step), the level is
// painted from those transforms, and the simulated state is put back bit for bit before anything
// else runs (b2Body::SetStateForDrawing, thirdparty/box2d). The camera (the Session node's
// position, moved by StageCamera::center once per step) is interpolated the same way.
//
// The simulation never sees an interpolated value: steps, controls, replays and the camera's own
// logic run exactly as without it, so the drawing is one step (1/30 s) behind the physics. Only
// levels on the browser profile use it; at 1/60 nothing changes.

class b2World;
class Session;

namespace cocos2d {
class Node;
}

namespace online {
namespace interp {

// True while the running level steps at 1/30 (online::browserPhysics()).
bool enabled();

// Session::setupLevel / level end: forget the previous level's poses.
void reset();

// Right before the world steps: put the camera back to its simulated position and record every
// body's pose (the "previous" end of the interpolation).
void beforeStep(b2World* world, cocos2d::Node* container);
// Right after the step's logic, paint and StageCamera::center: record the simulated camera.
void afterStep(cocos2d::Node* container);

// Move every body to its interpolated pose (alpha 0 = before the last step, 1 = now) for drawing.
void beginDraw(b2World* world, float alpha);
// Put every body moved by beginDraw back to its simulated state.
void endDraw(b2World* world);
// Place the camera between its last two simulated positions.
void drawCamera(cocos2d::Node* container, float alpha);

// True between beginDraw and endDraw: paint() code with side effects (random flicker, frame
// counters) skips them, so the extra paints change nothing but the picture.
bool drawing();

}  // namespace interp
}  // namespace online
