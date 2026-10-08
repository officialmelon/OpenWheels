#pragma once
// PC window management (Windows, Linux, macOS; not part of the original game).
//
// The original is a phone game: its scenes are laid out once, for the screen size they are created
// with (design resolution 3600x2000, FIXED_HEIGHT - the design width follows the aspect ratio).
// On PC the window is resizable, starts maximized and can go fullscreen (QOL page or F11), so:
//   * the size is settled before the first scene exists (maximized / saved fullscreen applied
//     before Application::run()), so every scene is laid out for the real window;
//   * when the size changes later, the running scene is kept centred and its full-screen
//     backgrounds are stretched to the new width (no black bars); the next scene is laid out for
//     the new size. The main menu is simply rebuilt.

namespace cocos2d {
class GLViewImpl;
}

namespace openwheels {
namespace desktop {

// startMaximized: maximize the window (default unless --width/--height were given).
// interactive: false for the --dump-world verification runner (no F11, no resize handling).
void installWindowManagement(cocos2d::GLViewImpl* glview, bool startMaximized, bool interactive);

}  // namespace desktop
}  // namespace openwheels
