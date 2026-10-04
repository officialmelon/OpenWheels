# Quality of Life options

Options → "quality of life" opens a page in the style of the original Advanced Options (the row
replaces "restore purchases"; there is no store). Code: `src/qol/`; hooks in `src/game/` are
marked `// QOL (PC addition):`. Every option defaults to the original game's behaviour and is
stored in UserDefault under `qol_*`.

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
| child gore | on / off | Gore for Irresponsible Dad's kid (and Irresponsible Mom's kids, `docs/RESTORED.md`). Shows "no art" when the gore sheet was not generated. Also respects the global gore setting. |
| fullscreen | off / on | Desktop only (also F11 by default). |
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

### Controls page

`src/qol/KeyBindings.*` holds the bindings (UserDefault `qol_keys_<action>`, only for actions that
differ from the defaults); `src/platform/win32/PCInput.cpp` and the fullscreen key read them. Click a
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

The keyboard bridge (`src/platform/win32/PCInput.cpp`) presses the original on-screen buttons. Default
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
