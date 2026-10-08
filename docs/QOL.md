# Quality of Life options

Options → "quality of life" opens a page in the style of the original Advanced Options (the row
replaces "restore purchases"; there is no store). Code: `src/qol/`; hooks in `src/game/` are
marked `// QOL (PC addition):`. Every option defaults to the original game's behaviour (except
"any character", which only concerns user / online levels, and "re-grab vehicle", which only
acts when an ejected rider grabs his own vehicle) and is stored in UserDefault under
`qol_*`.

The page has two columns, "visuals" and "game", and a "controls" page (desktop).

| Option | Values | What it does |
|---|---|---|
| blood | classic / streaks / liquid / realistic | After the browser game's four blood settings. Classic = the mobile particles. Streaks stretch each drop along its velocity. Liquid draws the blood emitters offscreen (`BloodCompositor`) and merges them with a blur + threshold shader; realistic adds gloss and shading. |
| max particles | 2000 / 4000 / 8000 | The session's particle budget (`Session::canAddEmitter`; original 2000). |
| camera | normal / far / farther / farthest | Gameplay zoom-out (scale 1, 0.8, 0.65, 0.5). The browser city backdrops scale with it (see below). |
| textures | auto / large / medium / small / tiny | The asset tier. Auto is the original's choice from the screen height and Options' "graphics: high-res / low-res" (a 1600x900 window gets medium); the others force a tier (`qol::overrideAssetTier`, AppDelegate). Like the original graphics option it is read at start-up: the row says "restart" until the game is restarted. |
| frame rate | 60 / 30 fps | After the browser game's 30 / 60 FPS option (v1.94). 30 draws every other frame; the game logic keeps its 1/60 s ticks (below). |
| fps counter | off / on | cocos2d-x stats display. |
| sound effects | 0-100 % | Slider (click or drag the row). Scales every sound effect on top of the original "sound: high / low / off" master volume (`Sound`, `SoundController`). |
| music | 0-100 % | Slider. Scales the menu music on top of the master volume; "intro music: off" still turns it off. |
| unlock all levels | off / on | Every campaign level is selectable. |
| any character | on / off | User and online (browser) levels that force a character still offer CHANGE CHARACTER (pause and victory menus, and CHANGE in the online browser); the picked character replaces the forced one until another level is selected (`qol/CharacterChoice.h`). On by default: these levels are not the original campaign, whose forced levels (and ghost races) always keep their character. |
| browser physics (online levels) | on / off | Browser (converted Flash) levels step their world once per 30 Hz Flash frame (1/30 s, 10 + 10 iterations, no block solver), as the browser game did; off plays them on the mobile profile (1/60 s, 8 + 3), as in OpenWheels 0.2. Campaign levels always step at 1/60. On by default (only concerns browser levels); a change applies from the next level. See "Browser physics" below and `docs/FLASH_LEVELS.md` 10.8. |
| child gore | on / off | Gore for Irresponsible Dad's kid (and Irresponsible Mom's kids, `docs/RESTORED.md`). Shows "no art" when the gore sheet was not generated. Also respects the global gore setting. |
| re-grab vehicle | on / off | An ejected rider who grabs his own vehicle gets back on and rides again (below). On by default; off is the original (a grabbed vehicle is just held). |
| fullscreen | off / on | Desktop only (also F11 by default). |
| touch controls | auto / show / hide | Desktop only: the mobile on-screen driving buttons. Auto (the default) hides them on desktop builds - the one option whose default is not the original's look; touch devices always show them. See "Keyboard" below. |
| controls | page | Desktop only: every keyboard action with its two keys, remappable (below). |

### 30 FPS

`Session::update` takes at most one fixed 1/60 s physics step per call (accumulator), and the
gameplay logic around it (controls, replay recording, timer, triggers, emitters) also runs once per
scheduler tick, so the frame rate cannot simply change. At 30 FPS (`qol::installFrameRate`) the
Director draws at 1/30 s, the Scheduler's time scale is 0.5 and a listener on
`Director::EVENT_AFTER_UPDATE` runs a second scheduler tick before the frame is drawn: every frame
is two ticks of half the frame time, the same 1/60 s ticks as at 60 FPS. Checked with
`OW_DUMP_FPS=30 OpenWheels.exe --dump-world ...` (two ticks per 1/30 s `mainLoop`), which writes
the same world as the 60 FPS run.

### Re-grab vehicle

After an ejection (eject button or a fall) the main character can grab (space / grab button) his
own vehicle to ride it again, in every level and for every character - the six mobile ones and the
five restored ones. Only the main rider gets back on (the kid, the girl, the elves, the daughter
and the son stay off). A grab counts when a hand that is still on his arm touches a body of the
vehicle (a contact, or for parts that do not collide with him, like the lawnmower, an overlap
while grabbing) at least 0.5 s after the ejection (summed physics steps). He must be alive and not
bleeding out, without a head / chest / pelvis smash or torso / neck break, the vehicle must not be
smashed, and he must still have what its `checkStateOfCharacter` needs (e.g. a hand for the
segway and pogo stick, a foot for the bike). Lost limbs stay off the vehicle.

`Vehicle::qolCacheRider` stores, at the first `addCharacter`, every rider part's transform and the
vehicle's bodies and fixture filters. `Vehicle::qolTryRemount` plans the move (the rider's parts
back into their starting pose relative to the frame, whatever hangs on him carried along) and
refuses it when he is pinned to the level or would end up inside solid geometry. `qolMount` then
turns the vehicle into its starting frame for a moment, runs the vehicle's own attach code there
(its anchors are spawn-time world points), and moves everything back onto the vehicle's current
transform with the frame's velocity - the joints come out exactly as at the start, with no
anchor gap or impulse. Each vehicle's `qolRemount` undoes its eject (flags, filters, sprites,
frame actions; the pedal cranks are turned back to their starting angle), and the limb injuries
are replayed through `handleInjury`. Then the driving controls come back and
`"characterRemounted"` is dispatched (Gameplay listens for the next ejection again, the race
status goes back from Ejected to Racing). Deterministic (no wall clock); `--dump-world` runs with
it off (`qol::suspendRegrabVehicle`). A replay recorded with the option on must be played with it
on.

Browser and user levels: the fall-off check (camera focus below -20 m sends `"characterDead"`
and stops the controls) counts from 20 m below the level's lowest fixed shape instead, since their
geometry may lie far below y = 0 (`Gameplay::checkCharacterPosition`).

### Browser physics

`online/FlashPhysics.*` (`online::browserPhysicsOption`, UserDefault `qol_browser_physics`). The
step is latched when a browser level starts (`LevelB2D::addInfo`). With the 1/30 step not every
scheduler tick steps the world, so in browser levels `Gameplay::update` runs the controls and the
timer only on ticks that will step (`Session::onlineWillStep`): one control byte and one timer
increment per world step, as at 1/60. The 30 FPS option still works: its two 1/60 s ticks per
frame take one 1/30 s step between them.

### Controls page

`src/qol/KeyBindings.*` holds the bindings (UserDefault `qol_keys_<action>`, only for actions that
differ from the defaults); `src/platform/desktop/PCInput.cpp` and the fullscreen key read them. Click a
key slot, then press the new key: Esc cancels, Backspace / Delete clears the slot, a key bound
elsewhere moves to the new action. "reset to defaults" restores the keys below. Android: the engine's
GL view forwards only back / menu / d-pad / enter keys and the keyboard bridge is Win32-only, so
remapping is a desktop option.

### City background and camera zoom

The browser city backdrops (`online/FlashCity.*`) are drawn at the gameplay zoom and follow the
camera like Flash's backdrops follow its container; see `docs/FLASH_LEVELS.md` 10.5.

## Child gore art

The mobile port shipped Irresponsible Dad's kid without gore art (his physics data is
complete). `tools/assets/extract_kid_gore.py` renders the missing frames (damage states, chunks,
organs, wounds) from the player's own decrypted browser-game SWF (by default
`binary/flash/swf/game_e_v1_87_g.dec.swf`) into `generated/<tier>/characters/`, calibrated
against the dad's mobile frames. It runs as a post-build step (CMake; Gradle task
`owGenerateKidGore`) and is skipped quietly without the SWF, FFDec or Java. Nothing extracted
is committed.

## Keyboard (desktop)

The keyboard bridge (`src/platform/desktop/PCInput.cpp`) presses the original on-screen buttons. Default
keys (remappable on the controls page):

| Action | Keys |
|---|---|
| accelerate | up / W |
| brake / reverse | down / S |
| lean back | left / A |
| lean forward | right / D |
| primary action (grab when ejected) | space |
| extra action 1 / 2 (restored characters, user vehicles) | left / right shift, left / right ctrl |
| eject | Z |
| pause | Esc / P |
| restart | R |
| fullscreen | F11 |

The bridge keeps every held key pressed across layout changes: once per frame (`tick`) a key whose
virtual finger sits on a button that went away (eject, death, victory, restart, pause) is lifted
off screen and pressed again on the current layout, so e.g. space held through an eject grabs at
once, and Esc / R held before the pause / reset buttons are back fire on release once they are.
No finger goes down while the controls layer is hidden (pause menu). On desktop the pause and reset
buttons come back 0.5 s after death instead of the original's 3 s ad wait
(`GameplayControls::handleDeath`).

### Hidden touch controls

With "touch controls" hidden (`qol::touchControlsShown`), `GameplayControls::addControls` marks
every state button key-only (`GameplayBtn::setKeyOnly`): it stays laid out where the original puts
it and the keyboard bridge presses it as before, but it is drawn at opacity 0 (pressed or not) and
`touchBegan` / `touchMoved` ignore real touches on it, so mouse clicks do not drive (they still work
on pause, reset and the menus). PCInput flags its own fingers (`qol::setKeyboardTouch`) because
GLView renumbers touch ids. The pause button, the timer, the moped / Santa boost meter and the
post-death reset button stay; desktop builds label the reset button with its key ("R",
`qol::createKeyHintLabel`). The campaign tutorial arrows (`Gameplay::highlightSpriteAtPos`) still
point where the button would be, with the bound keys ("UP / W", "SPACE"...) under the arrow.
