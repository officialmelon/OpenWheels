# Quality of Life options

Options → "quality of life" opens a page in the style of the original Advanced Options (the row
replaces "restore purchases"; there is no store). Code: `src/qol/`; hooks in `src/game/` are
marked `// QOL (PC addition):`. Every option defaults to the original game's behaviour and is
stored in UserDefault under `qol_*`.

| Option | Values | What it does |
|---|---|---|
| blood | classic / streaks / liquid / realistic | After the browser game's four blood settings. Classic = the mobile particles. Streaks stretch each drop along its velocity. Liquid draws the blood emitters offscreen (`BloodCompositor`) and merges them with a blur + threshold shader; realistic adds gloss and shading. |
| max particles | 2000 / 4000 / 8000 | The session's particle budget (`Session::canAddEmitter`; original 2000). |
| camera | normal / far / farther / farthest | Gameplay zoom-out (scale 1, 0.8, 0.65, 0.5). |
| fps counter | off / on | cocos2d-x stats display. |
| unlock all levels | off / on | Every campaign level is selectable. |
| child gore | on / off | Gore for Irresponsible Dad's kid (and Irresponsible Mom's kids, `docs/RESTORED.md`). Shows "no art" when the gore sheet was not generated. Also respects the global gore setting. |
| fullscreen | off / on | Desktop only (also F11). |
| keyboard controls | — | Desktop only: shows the key bindings. |

## Child gore art

The mobile port shipped Irresponsible Dad's kid without gore art (his physics data is
complete). `tools/assets/extract_kid_gore.py` renders the missing frames (damage states, chunks,
organs, wounds) from the player's own decrypted browser-game SWF (by default
`binary/flash/swf/game_e_v1_87_g.dec.swf`) into `generated/<tier>/characters/`, calibrated
against the dad's mobile frames. It runs as a post-build step (CMake; Gradle task
`owGenerateKidGore`) and is skipped quietly without the SWF, FFDec or Java. Nothing extracted
is committed.

## Keyboard (desktop)

The keyboard bridge (`src/platform/win32/PCInput.cpp`) presses the original on-screen buttons:
up/W accelerate, down/S brake/reverse, left/A lean back, right/D lean forward, space primary
action, Z eject, shift/ctrl the restored characters' extra actions, Esc/P pause, R restart.
