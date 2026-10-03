# Module ownership

> Layout note: since the reconstruction was completed, the files live in subsystem folders
> (`src/game/{app,audio,services,session,render,level,items,triggers,characters,vehicles,gameplay,menus,debug}/`).
> Every folder is on the include path, so includes stay flat (`#include "LevelB2D.h"`).
> The module table below records who reconstructed what.

Each module owns `src/game/<Class>.h/.cpp` for the classes listed. Only edit files you own.
If you need something from another module's class, use its public methods (declared from the
original symbols). If a header you need does not exist yet, forward-declare and move on; if a
base/shared header is wrong, note it in your report instead of editing it.

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
