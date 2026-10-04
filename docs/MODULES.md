# Module ownership

The reconstruction of `src/game/` was split into ten modules (M1-M10), each done by one agent
following `docs/RECONSTRUCTION.md`. The level editor port was split the same way into E1-E5
(`docs/EDITOR_PORT.md`). This page records the split and the shared base-class layouts.

**Work notes.** `docs/modules/M*.md` and `docs/editor/E*.md` are the modules' reconstruction
work logs, kept for the facts they record (field layouts, address-level findings, quirks kept
on purpose, parity explanations). They are historical: all modules are complete. In them,
*Phase H* means writing and verifying the headers (layouts, vtables) and *Phase I* means
implementing every function; "coordinator" and "integrator" refer to the agent that merged the
modules. Scripts they mention under `build/tmp/` were throwaway helpers and are not in the repo.
iOS export paths are written as `$OW_IOS_EXPORTS/ns_<Class>.c` (see `tools/re/iosre.py`).

Files live in subsystem folders
(`src/game/{app,audio,services,session,render,level,items,triggers,characters,vehicles,gameplay,menus,debug}/`).
Every folder is on the include path, so includes stay flat (`#include "LevelB2D.h"`).
While the work was in progress each module owned the files of its classes and edited no
others; cross-module needs went through public methods or were reported.

Approximate arm64 code size in instructions is given to show where the weight is.

| Module | Classes | ~insns |
|---|---|---|
| **M1 character-core** | CharacterB2D | 22.4k |
| **M2 bodies+vehicle-base** | Vehicle, PersonalTransporter, Wheelchair, BusinessGuy, IrresponsibleDad, WheelchairGuy, EffectiveShopper, PogostickGuy, MopedCouple, SpinalCord, IntestineChain, Ligament, CharacterSprite, CharacterData, Composite | 17.4k |
| **M3 vehicles** | Moped, MotorCart, RoadBike, PogoStick | 22.3k |
| **M4 level** | LevelB2D, LevelXMLParser, LevelXMLParserDelegateProtocol, LevelDataElement, TerrainNode, TerrainShape, ShapeItem, ShapeItemDelegate, CircleShape, RectangleShape, PolygonShape, TriangleShape, CircleArtShape, PolygonArtShape, GroupArtShape, GroupItem, Backdrop, BackgroundLayer | 21k |
| **M5 session-core** | LevelItem, Session (+ `SessionMode`), StageCamera, ContactListener, DestructionListener, QueryCallback, TargetRaycast, ReplayData, FFDrawNode, FFDrawNodeDelegate, LevelItemsDrawNode, LevelItemsDrawNodeWreckingBallDelegate, FFHelper, Emitter, EmitterDelegate, EmitterNode, BurstEmitter, FlowEmitter, Trigger, TargetActionBase, TargetAction, TargetActionGroup, TargetActionPrisJoint, TargetActionRevJoint, TargetActionSpecial, TargetActionTrigger | 19.5k |
| **M6 items-weapons** | Arrow, ArrowGun, HarpoonGun, Harpoon, BladeWeapon, HomingMine, Jet, Spikes, Mine, Van, WreckingBall | 17.7k |
| **M7 items-misc+gameplay** | BoostPanel, SpringBox, Log, Fan, IBeam, FinishLine, Sign, Bottle, SoccerBall, Token, Chain, SlowMotionPanel, Gameplay, GameplayControls, GameplayBtn, GameplayTimer, PauseLayer, VictoryMenu, VictoryAnimation, DyingVignette | 18.7k |
| **M8 menus-A** | MainMenu, LevelSelectMenu, LevelSelectBtn, CharacterSelectLayer, PerspectiveCharacters, PageControl, PageControlDelegate, Mascot, MenuHelper, SpriteButton, HighlightSprite | 13.1k |
| **M9 menus-B+windows** | HWWindow, HWWindowDelegate, SecondaryMenu, OptionsMenu, OptionsMenuItem, AdvancedOptionsMenu, InfoMenu, Credits, CreditsLayer, PrivacyPolicyScene, ResetWorkaroundScene | 10.7k |
| **M10 infrastructure** | AppDelegate, Settings, UserProgress, SoundController, Sound, SoundList (table extraction handled separately — implement the class API, see notes), Obfuscation, Tracker, AdController, AdControllerDelegate, IAPController, IAPControllerDelegate, DebugLayer, DebugScene, B2DebugDrawLayer, GLESDebugDraw, Test, UITest, `globals::` namespace data | 15k |

## Shared base classes (arm64 `nvsize` — where the derived class's own fields begin)

| Base | Owner | Own fields end (derived fields start at) | Notes |
|---|---|---|---|
| `cocos2d::Ref` | engine | 0x21 → 0x28 | |
| `cocos2d::Node` | engine | 0x2f8 | |
| `cocos2d::Layer` | engine | 0x31d → 0x320 | |
| `cocos2d::Sprite` | engine | 0x52d → 0x530 | |
| `cocos2d::DrawNode` | engine | 0x45c → 0x460 | |
| `cocos2d::ParticleSystemQuad` | engine | 0x5f8 | |
| `cocos2d::MenuItemLabel` | engine | 0x348 | |
| `LevelItem : cocos2d::Ref` | M5 | 0x94 → 0x98 | `CharacterB2D`'s `EmitterDelegate` sub-object sits at 0x98, `WreckingBall`'s `LevelItemsDrawNodeWreckingBallDelegate` at 0x98 |
| `TargetActionBase : LevelItem` | M5 | see `TargetAction` | no out-of-line code of its own |
| `Vehicle : LevelItem` | M2 | ~0x1b4 → 0x1b8 | |
| `CharacterB2D : LevelItem, EmitterDelegate` | M1 | 0x510 (`BusinessGuy` etc. are exactly 0x510) | `IrresponsibleDad`/`MopedCouple` = 0x518 |
| `ShapeItem : FFDrawNodeDelegate` | M4 | 0x40 | not a `Ref` |
| `Emitter : cocos2d::ParticleSystemQuad` | M5 | ~0x6b0 | `BurstEmitter` 0x6f0, `FlowEmitter` 0x700 |
| `SecondaryMenu : cocos2d::Layer` | M9 | 0x350 (sizeof) | `OptionsMenu`, `InfoMenu`, `AdvancedOptionsMenu` add `HWWindowDelegate` at 0x350 |
| `HWWindowDelegate`, `AdControllerDelegate`, `IAPControllerDelegate`, `PageControlDelegate`, `EmitterDelegate`, `FFDrawNodeDelegate`, `ShapeItemDelegate`, `LevelXMLParserDelegateProtocol`, `LevelItemsDrawNodeWreckingBallDelegate` | see table | 8 (vptr only) | pure interfaces; check their vtables with `owre.py vtable` |

Class sizes from `owre.py class` are heuristic (taken from the first `new` in a factory) and are
wrong for abstract bases — trust the table above and the `create()` of the concrete class.
