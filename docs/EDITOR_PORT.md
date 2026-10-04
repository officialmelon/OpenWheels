# Level editor port (iOS 1.2.7 → OpenWheels)

The Android build never shipped the level editor; the iOS build (the original Objective-C
codebase on cocos2d-iphone + UIKit) did. OpenWheels ports it to C++ on cocos2d-x 3.17.2, on
top of the Android-derived reconstruction in `src/game/`. Status: done, in `src/editor/`; the
per-part work notes are `docs/editor/E1.md`-`E5.md`.

Unlike `src/game/` (a 1:1 decompilation of the Android C++), this is a **port**: the iOS
editor's *behaviour* is reproduced faithfully, but it is expressed against our engine API and
game classes, and its UIKit screens are rebuilt with cocos2d-x UI.

## Using it

The editor needs the player's own iOS 1.2.7 app bundle (`happywheels.app`) for its art and
text. It is found automatically at `binary/HappyWheels_iOS/Payload/happywheels.app` (searched
upwards from the exe) or given with `--ios-app <dir>`; the Android build packs it from
`OW_IOS_APP` (`docs/ANDROID.md`). When the bundle's editor atlases load, the main menu shows two
extra buttons: the editor (new level) and the user-level list. Options → advanced options gets
an "import levels" row. A `.happywheels` file can be opened with `--open <file>`, passed as a
plain command-line argument, or dropped on the exe (Windows).

User levels are stored as `<writable path>/levels/<id>.xml` (imported ones under
`levels/imported/`) plus an `index.plist` with names and dates, replacing iOS Core Data
(`LevelMO`). Sharing writes `levels/shared/<name>.happywheels` and `<name>.xml`.

## Ground truth

* `python tools/re/iosre.py classes|methods|fn|ivars|senders ...` — iOS per-class decompilation
  (Ghidra exports of the iOS binary, folder in `OW_IOS_EXPORTS`) and ObjC ivar metadata.
* The editor writes the **same level XML** the game reads: `<levelXML><info v x y c f h bg bgc
  e="1" fm …/><shapes><sh t i p0 …/></shapes><specials><sp t p0 …/></specials>…` (see
  `-[EditorLayer levelData]`). `LevelB2D` loads it unchanged, so editor levels play in the
  reconstructed game. (Editor levels are y-up metres; `LevelB2D` flips y only for `r="1"`
  levels, which all shipped levels are.)
* Text: the iOS editor gets its strings from `Localizable.strings` (keys such as
  `EDITOR INTRO MESSAGE`, `EDITORBTN 0..12`, `LEVEL EDITOR`, `EXIT EDITOR`). The port reads the
  same keys at runtime from the player's iOS app bundle through `Localization::get("KEY")`;
  long hard-coded iOS strings use `OW_IOSTEXT(key, addr)`, extracted from the player's iOS binary
  at build time. Never hard-code that text.
* Art: `editorui-{hd,ipad,ipadhd}.plist/.png` and `levelEditorObjects1-{…}` in the player's iOS
  app bundle, loaded at runtime via `EditorAssets` (suffix picked from the asset tier). The
  bundle's binary plists and CgBI PNGs are read by `BinaryPlist` and `AppleImage`.

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
   `std::string`; `CGPoint/CGRect` (double) → `cg::Point/cg::Rect` where the double arithmetic
   changes results; `NSUndoManager` → `EditorUndoManager` (C++ command stack with grouping);
   delegates/notifications → interfaces / `uikit::NotificationCenter`; `UIAlertView` → the
   game's own `HWWindow`; popovers/view controllers → modal cocos2d-x panels (`UIKitCompat`).
4. **iOS game-class calls map to the Android reconstruction**: `Session::sharedSession()` /
   iOS `Settings` → our `Settings`/`Session`; `HWSoundController` → `SoundController`;
   `GameplayLayer` test play → `Gameplay::createScene(xml, nullptr)` with
   `GameplayControls::setMode(ControlsModeTesting)` (mode 1 already exists in the Android code
   for exactly this purpose; pause returns to the editor, reset restarts the test). Changes
   inside `src/game/` are minimal, marked `// EDITOR (iOS port):`, and do not alter behaviour
   when the editor is not involved.
5. **No game content in the repo** — same rules as `docs/RECONSTRUCTION.md` (no text, art,
   levels or decompiler output in `src/`).

## Layout

```
src/editor/model/        Special, RefShape (+Circle/Rectangle/TriangleRefShape), CharacterRef,
                         DecorationRef, EditorSettings (item catalog), the per-item *Ref classes
src/editor/core/         EditorLayer, EditorSpriteBatchNode, EditorLayerButton, EditorUndoManager,
                         CCLayerPanZoom, EditorLevelXMLParser, ButtonWithBatchedSprite, EditorGeometry
src/editor/ui/           EditorUIView, AddSpecialItemUIView, SelectBackgroundUIView,
                         EditParametersView, AlignRefsView, InputObject family,
                         EditorMenuViewController, EditorViewController, UIKitCompat
src/editor/persistence/  LevelSession, LevelStore, LevelMO (file-backed), LoadLevelViewController,
                         SaveLevelViewController, SBSaveLevelViewController,
                         UserLevelSelectUIView, LevelListView, LevelTextView, ShareAction
src/editor/flash/        browser-level features (PC addition, see below)
src/platform/common/     Localization, EditorAssets, IOSBundle, BinaryPlist, AppleImage
```

Not ported: `EditorMenuTableViewController` and `SettingsInputObject`/`SettingsSlider`, which
nothing in iOS 1.2.7 reaches (`docs/editor/E4.md`).

`cmake -DOW_WITH_EDITOR=OFF` builds without `src/editor/` (`tools/parity/EditorlessStubs.cpp`
supplies the few symbols the menus reference); used for the parity build tree.

## Browser-level features (PC addition)

Not in any shipped build: the editor is extended with everything the browser (Flash) editor
had, so levels from the online browser can be opened, remixed and saved. All of it lives in
`src/editor/flash/`; changes elsewhere in `src/editor/` are marked
`// EDITOR (browser features, PC addition)`. `src/game/` is untouched.

* **Format.** The editor now saves the **browser format** (Flash `SaverLoader.createXML`: px,
  y down, 20000x10000 stage, `<groups>`, `<joints>`, `<triggers>` with per-target action lists)
  with `<info … e="1" ow="1">`. Play-test, user-level play and the user library run it through
  `FlashLevelConverter` (as online levels are), so the game reads only one extra path. `ow="1"`
  tells the converter to keep the mobile backgrounds (3, 4, 4001), mobile-only special params
  and the iOS 5001 item. Old iOS-format levels still load (`FlashLevelIO::isBrowserLevelXml`
  picks the reader) and are upgraded on their next save.
* **Items.** `FlashCatalog` describes every browser special (props in XML order, ranges,
  labels, defaults, art), drawn by `FlashSpecialRef` with art rendered from the SWF at build
  time (`tools/assets/flash_items/editor.txt`): NPCs, text boxes, signs, food, furniture,
  cannon, chain, paddle, token, buildings and the rest. Characters include the 5 restored
  ones; backgrounds include City. Polygon and art shapes (`PolygonRefShape`, art bezier
  handles kept as loaded).
* **Logic.** `TriggerRef` (region, triggered by, repeat, delay, sound, start disabled, targets
  each with an action list and parameters, drawn links), `JointRef` (pin / sliding: limits,
  motor, collide, vehicle-controlled; bodies picked from what lies under the joint, drawn
  arms), `GroupRef` (logical groups, members stay editable; group properties, make vehicle).
* **UI.** `Inspector` (properties / trigger targets / actions, online-browser style, replaces
  `EditParametersView`), `ItemPalette` (replaces `AddSpecialItemUIView`), link mode for
  trigger targets, polygon/art tool.
* **PC input.** Click / shift-click / marquee select, drag, Ctrl+C/V/D/Z/Y (Shift+Z), Ctrl+A,
  Ctrl+G / Shift+G (group / ungroup), Delete, arrows (Shift: 10 px), mouse-wheel zoom,
  right-drag pan, Esc; keyboard-editable fields. Touch works as before.
* **Online.** The online browser's detail panel gets an EDIT button
  (`online::setOpenInEditorHandler` → `flashed::openLevelInEditor`); `--edit <file.xml>`
  opens a level from the command line.

## Verification

* Every `src/editor/*.cpp` passes `tools/check_tu.sh` and the MSVC and Android builds.
* Editor-made levels load in `LevelB2D` and play; test-play returns to the editor.
* Intended but not recorded as done systematically: a round trip of every shipped level's XML
  through the editor (`levelData()` attribute-for-attribute, within the iOS number formatting).
