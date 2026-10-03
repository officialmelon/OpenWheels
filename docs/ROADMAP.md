# Roadmap

## Phase 1 — Android 1.1.3 parity (current)
Faithful reconstruction of every game function in `libMyGame.so` on cocos2d-x 3.17.2, built for
Windows. Verified with the static parity diff (`tools/re/parity.py`) and the arm64 emulation
oracle (`tools/re/emu.py`), which runs the original code to compare level loading / physics.

## Phase 2 — iOS-only features (planned)
The iOS build (1.2.7) contains features the Android port never shipped, most notably the
**level editor**, plus newer content (e.g. a moped chapter in `LevelXML/05_moped`).

What we know about the iOS binary:
* It is the *original* Objective-C codebase on cocos2d-iphone (the Android build is a C++
  port of it that kept the same class names: `CharacterB2D`, `LevelB2D`, `MopedCouple`, ...).
* C++ symbols are stripped, but Objective-C metadata keeps class and selector names, so Ghidra
  recovers methods like `-[EditorLayer addSingleItem:properties:]`.
* Editor pieces: `EditorLayer`, `EditorLayerButton`, `EditorSpriteBatchNode`, `EditorUIView`
  (+ `EditorUIViewLayerDelegate`), `EditorViewController`, `EditorMenuViewController`,
  `EditorMenuTableViewController`, UIKit nibs/storyboard for load/save/menu screens, saved
  levels stored as Core Data objects (`initWithLevelMO:`), `editorui_*.png` art.
* Per-class Ghidra exports of the iOS build already exist from the earlier attempt:
  `~/openwheels_old/ghidra/exports/happywheels/ns_<Class>.c`.

Approach: re-implement the editor's game-side classes in C++ against cocos2d-x with the same
class/method structure (mirroring how the Android port translated the rest of the game), and
rebuild the UIKit screens with cocos2d-x UI. Levels keep the same XML format, so editor output
plays in the Phase 1 game unchanged. Phase 1's 1:1 class layout is what makes this tractable.
