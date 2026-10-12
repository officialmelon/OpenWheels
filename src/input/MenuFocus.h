#pragma once
// PAD (PC addition): full controller control of every menu (not part of the original game).
//
// Focus navigation: the d-pad / left stick moves a glowing highlight between the buttons on
// screen (spatial navigation over the running scene: menu items, level buttons, the online and
// editor widgets, list rows...); "menu: select" (A) presses the highlighted button by injecting a
// touch at its centre, so every button behaves exactly as when tapped. Inside a popup window,
// dropdown or panel the highlight stays in it. Buttons scrolled out of a list's view are scrolled
// into it; the right stick scrolls lists; LB / RB (or pushing past the edge) turn the pages of
// the level select.
//
// "menu: back" (B) presses the screen's back button, cancels a popup or resumes from the pause
// menu (Start too). Pressing the right stick (R3) switches to a free pointer for anything that
// isn't a button (the level editor's canvas, sliders): the left stick moves it, A taps / drags.
// Left / right on a slider row steps its value.
//
// The highlight shows while the controller is in use and hides when the mouse, a touch or a key
// is used.

namespace openwheels {
namespace menufocus {

// Once per frame after pad::update() (input/PadInput.cpp). `gameplayActive`: driving, the
// controller drives the game instead.
void update(float dt, bool gameplayActive);

}  // namespace menufocus
}  // namespace openwheels
