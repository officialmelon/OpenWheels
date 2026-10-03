# Physics parity log (original vs reconstruction)

Running log of the per-level parity work: what differed, root cause, fix, verification.
Tools: `tools/re/compare_play.py` (batch; results in `reports/compare/<level>.{oracle,ours}_f<N>.json`,
`reports/compare/summary_f<N>.txt`), `tools/re/oracle.py play`, `OpenWheels.exe --dump-world`,
`tools/re/worlddiff.py`. World dumps list bodies in b2World order (= reverse creation order).

## Tooling changes

* `compare_play.py`: our dumps and the summary are now per frame count (`ours_f<N>.json`,
  `summary_f<N>.txt`). The oracle cache is keyed by frame count only: delete
  `reports/compare/*oracle_f<N>.json` when changing `--script` for the same N.
* `OpenWheels.exe --dump-world out.json ... --dump-at a,b,c` also writes `out_f<N>.json` at those
  frames (same naming as `oracle.py play --dump-at`), for bisecting the first differing frame.
* `WorldDump.cpp`: distance and gear joints now emit the same 24 `raw` words as the oracle
  (arm64 offset 128 after the b2Joint base). Before, worlddiff reported every distance/gear joint as
  "raw: missing on ours" (01_06 joints 27/28, 01_15 joints 54/55, 02_02 joint 64 ...) — these were
  NOT missing joints, only a dumper gap.

## Results (worlddiff tol 1e-4; logs in reports/)

| | 3 frames, `0:00` | 180 frames, `0:01,90:05,150:10` |
|---|---|---|
| baseline (before this work) | 35/73 | 9/73 |
| now (MSVC, build_parity) | 73/73 | 18/73 |
| experiment: level+characters+vehicles TUs built by clang-cl with FMA (see below) | - | 47/57 runnable (14 crash, see below) |

Bit-exact (`--tol 0`) for all 180 frames with the MSVC build: 01_10 pink_nightmare,
01_14 dirty_rocks. 02_04 bicycle_motorcross is bit-exact to frame 93 with MSVC and for all 180
frames with the clang-built TUs.

## Fixes

### BladeWeapon in a group: handle box placed at the item position (`src/game/items/BladeWeapon.cpp`)
* Symptom: every BladeWeapon on a group body (ninja stars, sword crosses...) had its density-1
  handle fixture shifted by the rotated handle rect origin; wrong I/localCenter
  (01_04 body[11], 01_05 body[4,5,7,8], 01_08 body[8], 01_09 body[1], 02_03 body[1], 02_11 body[50],
  02_15 body[38], 03_13 body[0], 04_01 body[0], 04_07 body[4], 04_10 body[56], 06 construction_destruction body[19] ...).
* Root cause: in `BladeWeapon::init` @00584cdc, group branch, the original calls
  `AffineTransformRotate` and `AffineTransformTranslate` for the handle but never copies their
  results back (asm @005851f4..0058522c: result buffer sp+8 is not stored to the transform at
  sp+0x110), so the handle position is `position + identity.tx/ty` = the item position.
  The blade's transform does copy back. Fix: discard the two results like the original.

### x87 float leaks on Win32: Box2D joint getters (`src/platform/compat/Box2DFloat.h`)
* Symptom: 01_10 pink_nightmare bit-exact (worlddiff `--tol 0`) up to frame 74; at frame 75 (the
  character is ejected: `CharacterB2D::eject` -> `resetJointLimits`) knee/hip/elbow limits differed
  in the last bits (`upper` = 0.0299999937 vs 0.030000001), then chaos.
* Root cause: cocos2d-x's prebuilt Win32 `libbox2d.lib` computes the return expression of
  `b2RevoluteJoint::GetJointAngle()` on the x87 stack (`flds; fsubs; fsubs; ret`, 53-bit) and
  MSVC's caller consumes the unrounded st(0) with `fsubrs`. arm64 rounds each float op, so
  `(a-b) - GetJointAngle()` differs. Same pattern (single op) in `GetJointSpeed`,
  `Get*ReactionTorque`, `GetMotorTorque/Force`, `b2WheelJoint` getters (only GetJointAngle/Speed are
  used by the game).
* Fix: `owb2::jointAngle(j)` / `owb2::jointSpeed(j)` (force-inlined, computed from the inline
  accessors in SSE float math = the arm64 Box2D expression), used at all 24 game call sites
  (CharacterB2D, WreckingBall, Vehicle, Moped, RoadBike, Wheelchair).
* Verification: pink_nightmare bit-exact for all 180 frames (script `0:01,90:05,150:10`).
* Note: MSVC/x86 also evaluates some float-returning inline helpers on x87 in *our* code
  (out-of-line copies of `b2Dot`/`b2Cross`, ~280 x87 arithmetic instructions across 83 game
  functions, see recommendation below). Only a compiler change removes those systematically.

### Private verification build tree (`build_parity/`)
* The shared `build/` tree is used by the editor agent and currently fails in `src/editor/`.
  `CMakeLists.txt` got `option(OW_WITH_EDITOR ... ON)`; with OFF, `src/editor/` is excluded and
  `tools/parity/EditorlessStubs.cpp` provides the few LevelSession/EditorLayer symbols the menus
  reference (never used by --dump-world). Configure once with
  `cmake -S . -B build_parity -G "Visual Studio 17 2022" -A Win32 -T v143 -DOW_WITH_EDITOR=OFF`
  (plus `-DCMAKE_GENERATOR_INSTANCE=...`), then `tools/build.ps1 -BuildDir build_parity`;
  `compare_play.py --exe build_parity/bin/OpenWheels/RelWithDebInfo/OpenWheels.exe`.

### Bezier terrain: fused multiply-adds (`LevelB2D::calculateBezier` @005d6af0)
* Symptom: 02_04 bicycle_motorcross chain-shape (curved terrain) vertices differed by 1 ulp at load.
* Root cause: the original fuses 8 of the Horner-form multiply-adds (fmadd); MSVC never fuses.
* Fix: the exact fused sequence spelled out with `std::fma` (register-level decode in the source
  comment). MSVC's x86 `std::fma(float)` is correctly rounded (checked on 200k random cases
  against exact rational arithmetic, incl. near-cancellation).

### Ligament::create @005e58c8: y step is zero in the original (`src/game/characters/Ligament.cpp`)
* Symptom: 01_14 dirty_rocks bit-exact to frame 86; at 87 (a limb breaks, a Ligament is made) the two
  ligament bodies' y positions differed (oracle: both at upperPoint.y).
* Root cause: asm @005e59e4 `mov v0.S[1], v1.S[1]` puts lowerPoint.y into the subtrahend, so the
  step is `(lower.x-upper.x, lower.y-lower.y)/3`: an original-game quirk we had "fixed". Positions
  are `fmla` (fused).
* Fix: reproduce the zero y step and the fused positions. SpinalCord::create / IntestineChain::create
  have the same fused positions; SpinalCord's chest point is `GetWorldPoint` with clang's contraction
  (`owb2::worldPoint`, decoded @0063151c..0063153c). dirty_rocks now bit-exact for 180 frames.

## Float drift: findings and recommendation

Remaining drift is dominated by compiler floating-point semantics, not reconstruction logic:
1. **Contraction.** The original game code (clang 17, -ffp-contract=on) fuses `a*b+c` inside one
   expression, including in Box2D/cocos2d *inline header* code compiled into game TUs
   (`b2Body::GetWorldPoint` -> `b2Mul`, `ApplyLinearImpulse`/`ApplyForce` -> `b2Cross`, `b2Dot`...).
   Example: 02_04 bicycle diverges at frame 94 in `RoadBike::leanForwardButtonPressed`, whose fused
   ops are all inside inlined `GetWorldPoint` + `ApplyLinearImpulse`. Per-site `std::fma` would mean
   hand-expanding Box2D inlines at hundreds of call sites (570 fused ops in game code,
   `python tools/parity/fma_index.py reports/drift/fma_index.txt`).
2. **x87 on Win32.** MSVC x86 evaluates some float return paths on the x87 stack (prebuilt
   libbox2d getters; out-of-line `b2Dot`/`b2Cross` copies; ~280 x87 arithmetic instructions in 83
   game functions). arm64 rounds every op to float.

Experiment (evidence): all 126 src/game TUs compiled with the NDK's clang-cl 18
(`--target=i686-pc-windows-msvc -mfma -mavx2 -ffp-contract=on`, otherwise the exact MSVC flags) and
linked into the MSVC build (scripts: tools/parity/clangify.py + mklink.py, hard-coded local paths; objects in
build_parity/clangobj_all). Mixing all of them crashes (clang 18 against MSVC STL 14.44 needs
`_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH`, and TerrainNode alone crashes), but with only
level/ (minus TerrainNode), characters/ and vehicles/ clang-built: 47 of the 57 levels that run are
identical at 180 frames (MSVC: 17-18/73), bicycle_motorcross bit-exact for 180 frames. The runs are
deterministic when they do not crash.

**Recommendation:** build the game (and ideally cocos2d-x, which the original also compiled with
clang) with Visual Studio's own clang-cl (`-T ClangCL`, the "C++ Clang tools for Windows"
component, clang 19, which matches the installed MSVC STL, so no version-mismatch hack), Win32, with
`/clang:-ffp-contract=on -mfma` (FMA3 required: Intel Haswell+/AMD Piledriver+; without `-mfma`
LLVM lowers fmuladd to separate mul+add and the benefit is lost). Contraction is decided by the
clang front end per source expression (llvm.fmuladd), so the same source gives the same fused ops as
the original. Keep the prebuilt Box2D (it has ~0 fused ops, like the original's clang-5 Box2D) and
keep `owb2::jointAngle/jointSpeed` for its x87-returning getters. The std::fma edits made here stay
correct under clang (identical results). Not done here (build overhaul is the coordinator's call).

## Remaining differences (180 frames, MSVC build)
* Body-count mismatches at 180 frames only (02_13 black_and_orange 141/139, 04_08 city 100/101,
  06_01 pogo tutorial 43/42, 07 aqua_blue 180/178): all identical at 3 frames; black_and_orange is
  bit-exact to frame 90, pogo tutorial to frame 25: drift consequences (breakage thresholds),
  not load bugs. Recheck under a clang build.
* 03_08 blue_industrial: bit-exact to frame 90; diverges at 95 (lean phase), coarse at 120: drift.
* With the clang experiment still DIFF: 01_14 (fixed since), 02_05 cell_escape, 03_08,
  04_14 castle, pogo levels (02-05, floating_island, construction_destruction): next candidates for
  bisection (PogoStick per-frame code: lean*/updateCOMValues, x87 in MSVC-built TUs).
* Joint `raw` words for a distance joint created in the very frame of a dump: ours shows
  uninitialised solver temporaries (oracle heap is zeroed), harmless; ignore such single diffs.
