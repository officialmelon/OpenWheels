# Level editor port (iOS 1.2.7 → OpenWheels)

The Android build never shipped the level editor; the iOS build (the original Objective-C
codebase on cocos2d-iphone + UIKit) did. This phase ports it into OpenWheels as C++ on
cocos2d-x 3.17.2, on top of the Android-derived reconstruction in `src/game/`.

Unlike `src/game/` (a 1:1 decompilation of the Android C++), this is a **port**: the iOS
editor's *behaviour* is reproduced faithfully, but it is expressed against our engine API and
game classes, and its UIKit screens are rebuilt with cocos2d-x UI.

## Ground truth

* `python tools/re/iosre.py classes|methods|fn|ivars|senders ...` — iOS per-class decompilation
  (Ghidra exports of the iOS binary) and ObjC ivar metadata.
* The editor writes the **same level XML** the game reads: `<levelXML><info v x y c f h bg bgc
  e="1" fm …/><shapes><sh t i p0 …/></shapes><specials><sp t p0 …/></specials>…` (see
  `-[EditorLayer levelData]`). Our `LevelB2D` loads it unchanged, so editor levels play in the
  reconstructed game, and the same path serves custom levels / future level importers.
* Text: the iOS editor gets its strings from `Localizable.strings` (e.g. `EDITOR INTRO MESSAGE`,
  `EDITORBTN 0..12`, `LEVEL EDITOR`, `EXIT EDITOR`). The port reads the same keys at runtime from
  the player's iOS app bundle through `Localization::get("KEY")` — never hard-code that text.
* Art: `editorui-{hd,ipad,ipadhd}.plist/.png` and `levelEditorObjects1-{…}` in the player's iOS
  app bundle, loaded at runtime via `EditorAssets` (suffix picked from the asset tier).

## Rules

1. **Behaviour-faithful.** Property sets and order (`propertyKeys`, `propertyKeysForUI`),
   defaults, min/max ranges, snapping/rotation math, selection and marquee logic, undo/redo
   semantics, shape/art count limits, copy/paste/align, XML output (attribute names, order and
   number formatting: integers vs `%.02f`), intro/alert flow — exactly as the iOS code does.
2. **Names mirror iOS.** Classes keep their iOS names (`EditorLayer`, `EditorSpriteBatchNode`,
   `Special`, `RefShape`, `ArrowGunRef`, `InputObject`, …), selectors become camelCase methods
   (`addSingleItem:properties:` → `addSingleItem(int, const cocos2d::ValueMap&)`). Each method
   carries the iOS address tag: `// @ios 100005e48`.
3. **ObjC → C++ mapping.** `NSArray/NSMutableArray` of objects → `cocos2d::Vector<T*>`;
   `NSDictionary` properties → `cocos2d::ValueMap`; `NSNumber`/`NSString` → `cocos2d::Value` /
   `std::string`; `CGPoint/CGRect` (double) → `cocos2d::Vec2/Rect` (keep the double
   arithmetic where it changes results); `NSUndoManager` → `EditorUndoManager` (C++ command
   stack with grouping); delegates/notifications → interfaces / `EventCustom`; `UIAlertView` →
   the game's own `HWWindow`; popovers/view controllers → modal cocos2d-x panels.
4. **iOS game-class calls map to the Android reconstruction**: `Session::sharedSession()` /
   iOS `Settings` → our `Settings`/`Session`; `HWSoundController` → `SoundController`;
   `GameplayLayer` test play → `Gameplay::createScene(xml, nullptr)` with
   `GameplayControls::setMode(ControlsModeTesting)` (mode 1 already exists in the Android code
   for exactly this purpose). Changes needed inside `src/game/` must be minimal, marked
   `// EDITOR (iOS port):`, and must not alter behaviour when the editor is not involved.
5. **No game content in the repo** — same rules as `docs/RECONSTRUCTION.md` (no text, art,
   levels or decompiler output in `src/`).

## Layout

```
src/editor/model/        Special, RefShape (+Circle/Rectangle/TriangleRefShape), CharacterRef,
                         DecorationRef, the per-item *Ref classes
src/editor/core/         EditorLayer, EditorSpriteBatchNode, EditorLayerButton, EditorUndoManager
src/editor/ui/           EditorUIView, AddSpecialItemUIView, SelectBackgroundUIView,
                         EditParametersView, AlignRefsView, InputObject family,
                         EditorMenuViewController/TableViewController, EditorViewController
src/editor/persistence/  LevelMO (file-backed), LoadLevelViewController, SaveLevelViewController,
                         SBSaveLevelViewController, UserLevelSelectUIView
src/platform/common/     Localization, EditorAssets (shared runtime helpers)
```

User levels are stored as `<writable path>/levels/<id>.xml` plus a small index (name, dates),
replacing iOS Core Data (`LevelMO`).

## Verification

* Every `src/editor/*.cpp` passes the MSVC build.
* Round trip: loading any shipped level's XML into the editor and writing it back with
  `levelData()` yields the same shapes/specials (attribute-for-attribute, within the iOS
  number formatting) for every item type the iOS editor supports.
* Editor-made levels load in `LevelB2D` and play; test-play returns to the editor.
