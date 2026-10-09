# Game controllers, rumble and tilt steering

OpenWheels can be played start to finish with a game controller on every platform: Windows,
Linux, macOS, Android (Bluetooth / USB pads and the built-in controls of handhelds like the AYN
Odin and Thor) and iOS. The controller drives the game, moves through every menu, and rumbles on
crashes. Phones, tablets and handhelds can also steer by tilting. None of this is part of the
original game. The code is in `src/input/` (marked `PAD (PC addition)` where it touches game code).

## Default controls

| Controller | Driving | Menus |
|---|---|---|
| Left stick / d-pad up | Accelerate | Move the highlight |
| Left stick / d-pad down | Brake / reverse | Move the highlight |
| Left stick / d-pad left / right | Lean back / forward | Move the highlight; left / right on a slider row changes it |
| RT / LT | Accelerate / brake | |
| A | Primary action (jump, jet, brake, fire; grab when ejected) | Press the highlighted button |
| B | Extra action 2 (Flash Ctrl) | Back / cancel / close |
| X | Extra action 1 (Flash Shift) | |
| LB / RB | Extra action 1 / 2 | Previous / next chapter (level select) |
| Y | Eject | |
| Start | Pause | Resume (pause menu) |
| Back / Select | Restart (after death) | |
| Right stick | | Scroll lists |
| R3 (right stick click) | | Free pointer on / off |

The extra actions are the restored characters' second buttons (Helicopter Man's rope up / down,
Explorer Guy's stand / crouch, Irresponsible Mom's kids) and a browser user vehicle's Shift / Ctrl
actions (jets, brakes, arrow guns).

Every driving action and the two menu actions are remappable on *Options → quality of life →
controller*: select one of an action's three slots, then press the button, trigger or stick
direction to put there. Selecting the waiting slot again clears it; nothing pressed for six seconds
cancels; "reset to defaults" restores the table above. A controller input is bound once among the
driving actions and once among the menu actions, so A can both jump and select.

## Menus

The d-pad or left stick moves a glowing yellow highlight between the buttons on screen, and A
presses the highlighted one: the press is a touch injected at its centre, so every button behaves
exactly as when tapped. Navigation is spatial: the nearest button in the pushed direction, lined up
with the current one where possible. It covers the game's menus, level select, character select,
the pause and victory menus, popup windows, the QoL pages, the online level browser, Your Levels,
the Nearby / race panels, account panels and dropdowns.

- Inside a popup window, a dropdown or a panel, the highlight stays in it.
- B presses the screen's back button, a popup's cancel / close (or its only button), closes a
  dropdown or panel (Escape), or resumes from the pause menu. Start also resumes.
- Buttons scrolled out of a list's view are scrolled into it when the highlight reaches them;
  pushing past the end of a list scrolls it; the right stick scrolls.
- On the level select, pushing past the last button, or LB / RB, turns the chapter.
- R3 switches to a free pointer for things that aren't buttons (the level editor's canvas,
  sliders, maps): the left stick moves it, A taps and drags, the right stick scrolls under it.
- The highlight shows while the controller is in use and hides as soon as the mouse, a touch or a
  key is used.

## Supported controllers

| Platform | How | Notes |
|---|---|---|
| Windows | GLFW 3.2 joysticks | XInput pads (Xbox 360 / One / Series and pads emulating them) and PlayStation pads (DirectInput "Wireless Controller") get the standard layout. Rumble through XInput. |
| Linux | GLFW 3.2 joysticks (joydev) | Xbox-style pads (xpad, Steam Input's virtual pad, most X-input mode pads) and PlayStation pads (hid-playstation) get the standard layout. Rumble through the event device's force feedback (needs write access to `/dev/input/event*`, which desktop distributions give the logged-in user for controllers). |
| macOS | GLFW 3.2 joysticks | Left stick on axes 0 / 1; other inputs raw (bind them on the controller page). No rumble. |
| Android | `InputDevice` key / motion events | Every controller Android knows uses the standard layout, including handheld built-in controls. Rumble through the controller's vibrator, or the device's for built-in controls. |
| iOS | GCController (cocos2d-x `Controller`) | MFi, Xbox and PlayStation pads. No rumble. |

Controllers of an unknown layout report raw buttons and axes ("button 7", "axis 3+"). Their left
stick (axes 0 / 1) moves through the menus; bind the rest on the controller page, with the mouse or
a touch if "menu: select" isn't bound yet.

On Android a controller's inputs are taken before cocos2d-x sees them, so a pad's B never becomes
the system Back and its d-pad never moves Android's focus. The device's own Back key keeps working
as before.

## Touch controls with a controller

*touch controls: auto* hides the on-screen driving buttons on desktop builds and, on phones,
tablets and handhelds, whenever a controller is connected (decided when a level's controls are
laid out). *show* and *hide* force them on every platform. With them hidden, the tutorial arrows
and the restart button show the controller's buttons instead.

## Rumble

The rumble follows the game's own sounds: every crash, hit, broken bone, lost limb and explosion
plays one, so SoundController reports them to `src/input/Haptics.cpp` and the sound's name picks
the strength (explosions strongest, then the character's injuries, smashes, then light hits),
faded by its distance from the camera. It only plays while driving.

- **rumble: off / low / medium / high** (default high) on the controller page.
- **phone vibration** (Android, off by default): the same haptics on the phone's own vibrator
  when no controller is connected. Handhelds' built-in controls use the device vibrator anyway.

## Tilt steering

**tilt steering: off / low / medium / high** (Android and iOS, off by default). Turning the
device like a steering wheel leans: left side down leans back, right side down leans forward.
The game's controls are on / off, so the lean is pressed once the device has turned past 20°
(low), 12° (medium) or 7° (high), and let go a little before it comes back. Tilt adds to the touch
and controller controls. Android reads the gravity sensor (fused with the gyroscope where the device
has one), iOS the accelerometer, both in screen space for the current orientation.

## How it works

- `src/input/Gamepad.{h,cpp}`: the controller core. A platform provider reports every
  controller's state once per frame (`src/platform/desktop/DesktopGamepad.cpp`,
  `src/platform/android/AndroidGamepad.cpp` + `GameInput.java`, `src/platform/ios/IosGamepad.cpp`);
  the core merges them into one set of held inputs with press / release edges (with hysteresis),
  times the rumble and runs the remap capture.
- `src/input/PadInput.cpp`: runs every frame before the game's updates. Driving actions press
  virtual fingers on the gameplay buttons through `src/input/ControlBridge.cpp`, which the
  keyboard bridge (`src/platform/desktop/PCInput.cpp`) shares. An action held while the game
  wasn't being driven (A held through "play") waits until it is let go. On browser levels the
  extra bits go to user vehicles directly (`online::setPadExtraBits`).
- `src/input/MenuFocus.cpp`: the focus navigation. Each scan walks the running scene in draw
  order and collects the buttons a finger could press (menu items, level buttons, the online UI
  widgets, cocos2d-x UI widgets, nodes named `ow_focus` such as list rows), clipped by scroll views
  and clipping nodes, restricted to the topmost popup (`HWWindow` or a node named `ow_modal`). The
  highlight is drawn on the Director's notification node, above every scene.
- `src/input/Haptics.cpp`, `src/input/Tilt.cpp`: rumble and tilt, as above.
- `src/qol/PadBindings.cpp`, `src/qol/QoLPadMenu.cpp`: the bindings (UserDefault `qol_pad_<n>`)
  and the controller page.
