#pragma once
// iOS EditorLayer : CCLayerColor (instanceSize 0x2e0) - the level editor scene: toolbar buttons,
// the pan/zoom stage with the item layer (EditorSpriteBatchNode), shape/art counters, the intro
// alert, copy/paste/lock/centre, the level XML writer (levelData) and test play.
//
// ---- Entry points -------------------------------------------------------------------------------
//   MainMenu "editor" button (iOS MainMenuLayer tag 4) -> EditorLayer::createScene()  (new level)
//   E5 user-level list "edit"/"new" (iOS UserLevelSelectUIView / LoadLevelViewController)
//       -> EditorLayer::createSceneWithLevelMO(levelMO) / createScene()
//   testLevel -> Director::pushScene(Gameplay::createTestingScene(levelData())); the game's
//       pause button pops back to this (still alive) scene (see docs/editor/E3.md, hooks).
//   exitEditor -> Director::replaceScene(MainMenu::createScene(MenuModeMain, nullptr)).
//
// ---- Coordinates (port) ---------------------------------------------------------------------------
// The iOS editor is laid out in iPad points (iPad branches of every UIDevice idiom test are
// used). This layer is scaled by EditorAssets::pointsToDesign() at the origin, so all its
// children (buttons, labels, rotCirc/moveCirc, the stage) use the iOS point values unchanged and
// "winSize" is uikit::windowSize() (768 points tall). Screen::notchOffset is uikit::notchOffset()
// (0 on PC). Sprites from 2x (-ipadhd) atlases get setScale(1 / EditorAssets::pixelsPerPoint());
// pointSize(node) gives the iOS point contentSize for layout maths.
// Stage space (CCLayerPanZoom content, EditorSpriteBatchNode): iOS uses iPad points with ptmRatio
// 72 (kPtmRatio; Session mode 2), stage 320 x 160 m. The port measures stage space in editor-atlas
// art units (stageUnitInPoints() points each) so all art shows at scale 1: ptm = stagePtmRatio(),
// and every iOS stage display scale (0.25, 0.75, max 3) is multiplied by stageUnitInPoints().
//
// ---- Touch order (port) -----------------------------------------------------------------------------
// iOS: UIKit panels first, then cocos2d targeted delegates by priority (later registration first
// among equals), then standard (multi-touch) delegates. cocos2d-x: fixed priority < 0, scene
// graph (uikit::View panels from E4), fixed priority > 0 (ascending; equal = registration order),
// then all-at-once listeners. Fixed priorities used by the editor core:
//   ButtonWithBatchedSprite::kTouchPriority  = 1  (iOS 0, registered after the batch node)
//   EditorSpriteBatchNode::kTouchPriority    = 2  (iOS 0)
//   EditorLayer::kTouchPriority              = 3  (iOS 1; ccTouchBegan always returns false)
//   CCLayerPanZoom::kTouchPriority           = 4  (standard delegate; all-at-once listener)
// EditorLayer also installs a fixed-priority -1000 non-swallowing listener that only calls
// EditorUndoManager::endEventGroup() before each touch phase (NSUndoManager groupsByEvent).
//
// ---- Notifications (uikit::NotificationCenter, iOS names) -------------------------------------
// Observed while on stage (addObservers in onEnter, all removed in onExit):
//   "sel_ref_change" -> selectionChanged, "undo_stack_updated" -> undoStackUpdated,
//   "ART_COUNT_UPDATE" -> updateArtCount, "SHAPE_COUNT_UPDATE" -> updateShapeCount (Special).
// Per panel: "editor_view_closed" (object = that panel; any object for EditParametersView)
//   -> hideMenu, "editor_view_item_added" (object = int* level item id) -> handleMenuTouch,
//   "save_level_done" -> saveLevelComplete, "main_menu_closed" -> removeMainMenu.
//
// ---- Limits ---------------------------------------------------------------------------------------
// 600 shapes, 1000 art (sum of Special::shapeCount()/artCount() over all refs); labels
// "<SHAPES LEFT>: n" / "<ART LEFT>: n" (Localization keys "SHAPES LEFT", "ART LEFT").

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "2d/CCLayer.h"
#include "base/CCValue.h"
#include "base/CCVector.h"
#include "EditorGeometry.h"
#include "EditorLevelXMLParser.h"
#include "EditorUIView.h"
#include "HWWindowDelegate.h"
#include "FlashLevelIO.h"  // EDITOR (browser features, PC addition)

namespace cocos2d {
class Label;
class Scene;
class Sprite;
class SpriteBatchNode;
class EventListener;
}

namespace uikit {
class View;
}

class CCLayerPanZoom;
class EditorLayerButton;
class EditorPopoverController;
class EditorSpriteBatchNode;
class EditorUIView;
class HWWindow;
class LevelMO;
class Special;

// Toolbar button tags (iOS setTag:, handleOnRelease: switch, "EDITORBTN <tag>" roll-over text).
enum EditorLayerButtonTag
{
    EditorLayerButtonTagMenu = 0,
    EditorLayerButtonTagTogglePan = 1,
    EditorLayerButtonTagAdd = 2,
    EditorLayerButtonTagTest = 3,
    EditorLayerButtonTagEditParams = 4,
    EditorLayerButtonTagTrash = 5,
    EditorLayerButtonTagAlign = 6,      // no button is created with this tag in 1.2.7
    EditorLayerButtonTagUndo = 7,
    EditorLayerButtonTagRedo = 8,       // no button is created with this tag in 1.2.7
    EditorLayerButtonTagCopy = 9,
    EditorLayerButtonTagPaste = 10,
    EditorLayerButtonTagSelectBg = 11,
    EditorLayerButtonTagExit = 12,      // no button is created with this tag in 1.2.7
};

namespace flashed {
class Inspector;
}
class TriggerRef;
class PolygonRefShape;

class EditorLayer : public cocos2d::LayerColor,
                    public EditorLevelXMLParserDelegate,
                    public EditorUIViewLayerDelegate,
                    public HWWindowDelegate,
                    public flashed::LevelReaderDelegate
{
public:
    static const int kTouchPriority;          // 3, see "Touch order"
    static const int kShapeLimit = 600;
    static const int kArtLimit = 1000;
    static const float kPtmRatio;             // 72 (iOS Session ptmRatio, mode 2, iPad)

    // ---- creation ---------------------------------------------------------------------------
    // +[EditorLayer scene]: Scene + [EditorLayer node] (init: stops the music, no LevelMO).
    static cocos2d::Scene* createScene();                                  // @ios 100005c2c
    // +[EditorLayer sceneWithLevelMO:]: Scene + initWithLevelMO: (music keeps playing).
    static cocos2d::Scene* createSceneWithLevelMO(LevelMO* levelMO);       // @ios 100005bd8
    static EditorLayer* create();
    static EditorLayer* createWithLevelMO(LevelMO* levelMO);

    // SoundController stopMusic, then initWithLevelMO(nullptr).
    bool init() override;                                                  // @ios 100004a7c
    // Builds the whole editor; with a LevelMO its data is loaded (addLevelItems), otherwise a
    // default character {x 75, y 50, c 2} is added; then centerToCharacter, pan off, intro alert.
    bool initWithLevelMO(LevelMO* levelMO);                                // @ios 100004ab0
    // "levelEditorObjects1" and "editorui" atlases (EditorAssets::loadAtlas).
    void loadAtlasFiles();                                                 // @ios 1000058e0
    void addObservers();                                                   // @ios 100005968
    // dealloc: removes observers                                           // @ios 100005a24
    // UserDefault "editor_intro_message_shown": first time an alert LEVEL EDITOR /
    // EDITOR INTRO MESSAGE / OK (HWWindow); the flag is then set unconditionally.
    void showIntroMessage();                                               // @ios 100005ac8
    void onEnter() override;                                               // @ios 100005c70
    void onExit() override;                                                // @ios 100005d14

    // ---- loading level data -----------------------------------------------------------------
    // Parses levelMO->data() with EditorLevelXMLParser (this = delegate), then clears the undo
    // stack and refreshes the undo/redo buttons.
    void addLevelItems();                                                  // @ios 100005dc0
    // Ref for `levelItemID` (Settings levelItemRefClass:), setProperties, create, sbn addRef.
    Special* addSingleItem(int levelItemID, const cocos2d::ValueMap& properties);  // @ios 100005e48
    void addSingleShape(int levelItemID, const cocos2d::ValueMap& properties);     // @ios 100005ee0  (empty)
    // EditorLevelXMLParserDelegate (iOS LevelXMLParser delegate methods):
    void setVersion(float version, unsigned int background, unsigned int color,
                    const cocos2d::ValueVector& parameters) override;     // @ios 100008770
    // CharacterRef (level item 5000): x/y metres, f, h, then default character max(c, 1).
    void addCharacter(const cocos2d::ValueMap& character) override;       // @ios 10000878c
    void addShape(const cocos2d::ValueMap& shape, unsigned int objectIndex) override;     // @ios 1000088f0  t + 6000
    void addSpecial(const cocos2d::ValueMap& special, unsigned int objectIndex) override; // @ios 100008934

    // ---- view ---------------------------------------------------------------------------------
    // stage scale 0.75, centre refs[0] at (0.25, 0.5) of the window (x, y fractions).
    void centerToCharacter();                                              // @ios 100005ee4
    void centerToRefWithIndex(unsigned int index, const cg::Point& normalPoint);  // @ios 100005f34
    void enable(bool enable);                                              // @ios 100006630
    void togglePan(EditorLayerButton* sender);                             // @ios 100007a58

    // ---- shape / art limits ---------------------------------------------------------------------
    // Alert ("Shape Limit Reached" / "Art Limit Reached" / "Shape/Art Limit Reached", "Ugh, fine!")
    // and false when the addition would exceed 600 shapes / 1000 art.
    bool showAlertIfExceedingShapeCount(unsigned int shapeCount, unsigned int artCount);  // @ios 100005ff0
    bool canAddItemWithShapeCount(unsigned int shapeCount, unsigned int artCount);       // @ios 100006538
    void updateArtCount();                                                 // @ios 100006358
    void updateShapeCount();                                               // @ios 100006448

    // ---- toolbar ------------------------------------------------------------------------------
    // AddSpecialItemUIView (E4) on the right; observes its close / item-added notifications.
    void addItemsBtnPressed(EditorLayerButton* sender);                    // @ios 1000060f4
    // iOS ccTouchBegan:withEvent: (cocos2d-x Layer reserves the cc* names as final).
    bool onTouchBegan(cocos2d::Touch* touch, cocos2d::Event* event) override;  // @ios 100006214  (returns false)
    // "editor_view_item_added": new ref of that id at the window centre (limits checked),
    // added through addRefs (undoable, selected).
    void handleMenuTouch(void* notification);                              // @ios 10000621c
    void selectionChanged(void* notification);                             // @ios 100006354  -> refreshButtons
    void undoStackUpdated(void* notification);                             // @ios 100006580
    void refreshButtons();                                                 // @ios 100006a20
    // Shows/enables every toolbar button except `exceptThese`.
    void setAllButtonsVisible(bool visible, const cocos2d::Vector<EditorLayerButton*>& exceptThese);  // @ios 100006bec
    void disableButton(EditorLayerButton* button);                         // @ios 100006d2c
    void enableButton(EditorLayerButton* button);                          // @ios 100006d78
    void undoBtnPressed(EditorLayerButton* sender);                        // @ios 1000072f0
    void redoBtnPressed(EditorLayerButton* sender);                        // @ios 1000072f4
    void copyBtnPressed(EditorLayerButton* sender);                        // @ios 1000072f8
    void pasteBtnPressed(EditorLayerButton* sender);                       // @ios 1000072fc
    // Button callbacks (ButtonWithBatchedSprite targets): roll-over label "EDITORBTN <tag>".
    void handleOnPress(EditorLayerButton* sender);                         // @ios 100007420
    void handleOnRollOff(EditorLayerButton* sender);                       // @ios 100007560
    void handleOnRelease(EditorLayerButton* sender);                       // @ios 100007580  (dispatch on tag)
    void removeRollOverLabel();                                            // @ios 1000076e8
    // Editor menu (E4 EditorMenuViewController; iPad popover from the menu button).
    void showMainMenuBtnPressed(EditorLayerButton* sender);                // @ios 100007720
    void removeMainMenu(void* notification);                               // @ios 10000791c
    // "editor_view_closed": re-enables editing, removes currentUIView, restores the undo button.
    void hideMenu(void* notification);                                     // @ios 1000079b4
    void alignBtnPressed(EditorLayerButton* sender);                       // @ios 100007bfc  AlignRefsView (E4)
    void trashBtnPressed(EditorLayerButton* sender);                       // @ios 100007d24
    void selectBgBtnPressed(EditorLayerButton* sender);                    // @ios 100007d5c  SelectBackgroundUIView (E4)
    void editParamsBtnPressed(EditorLayerButton* sender);                  // @ios 100007f24  EditParametersView (E4)
    void paramChanged(void* notification);                                 // @ios 1000080d4  -> sbn updateSelectionRect

    // ---- editing operations (also called by the E4 editor menu) -------------------------------
    // Locks every selected ref except the character (kept in lockedRefs), clears the selection.
    void lockSelection();                                                  // @ios 100006684
    void unlockAll();                                                      // @ios 1000067d4
    void setSnapToAngle(bool snapToAngle);                                 // @ios 1000068e0
    bool snapToAngle();                                                    // @ios 1000068f0
    void setRotateItemsIndependently(bool rotateIndependently);            // @ios 100006900
    bool rotateItemsIndependently();                                       // @ios 100006910
    void setLockToAxis(bool lockToAxis);                                   // @ios 100006920
    bool lockToAxis();                                                     // @ios 100006930
    // canUndo && !hasSaved.
    bool unsavedChanges();                                                 // @ios 100006940
    const cocos2d::Vector<Special*>& selectedRefs();                       // @ios 100006978
    // Copies the selection's properties dictionaries (character excluded); enables paste.
    void copySelection();                                                  // @ios 100006dc4
    // New refs from the copied properties (limits checked), addRefs; unless inPlace they are
    // centred on the window; counters updated.
    void pasteInPlace(bool inPlace);                                       // @ios 100006f24  pasteInPlace:
    void centerSelected();                                                 // @ios 100007150
    bool canUndo();                                                        // @ios 100007324
    void undo();                                                           // @ios 100007344
    void redo();                                                           // @ios 10000737c
    void reset();                                                          // @ios 1000073b4
    // (iOS editorUIView:changedValue:forKey:userInfo:, EditorUIViewLayerDelegate) - "bg" / "bgColor"
    // from SelectBackgroundUIView: bg 0 -> white, bg -1 -> custom userInfo["color"].
    void editorUIView(EditorUIView* view, const cocos2d::Value& value, const std::string& key,
                      const cocos2d::ValueMap& userInfo) override;         // @ios 100007e74

    // ---- level data / persistence / test play -------------------------------------------------
    unsigned int characterIndex();                                         // @ios 1000080e4  refs[0] defaultCharacter
    bool forceCharacter();                                                 // @ios 10000810c  refs[0] forceCharacter
    // The level XML, byte-for-byte as iOS writes it (see docs/editor/E3.md "levelData"):
    //   <levelXML><info v="1.70" x="%.02f" y="%.02f" c="%i" f="%i" h="%i" bg="%i" bgc="%i" e="1" fm="m"/>
    //   [<shapes>{<sh t="%i" i="%i" {p%i="%i" |p%i="%.02f" } />}</shapes>]
    //   [<specials>{<sp t="%i" {p...} />}</specials>]</levelXML>
    std::string levelData();                                               // @ios 100008140
    // iOS Session levelData {data, force_character, playable_character} + character index,
    // vehicle 0 -> sessionLevelData() and the game's Settings (see E3.md, "test play").
    void applyLevelDataToSession();                                        // @ios 1000085b8
    void testLevel(EditorLayerButton* sender);                             // @ios 1000086b4
    void exitEditor(EditorLayerButton* sender);                            // @ios 1000086f4
    // "save_level_done" (E5): hasSaved = true, dismiss the presented view controller.
    void saveLevelComplete(void* notification);                            // @ios 100006988
    // LevelMO = nullptr, hasSaved, sbn reset, centre, counters.
    void newLevel();                                                       // @ios 1000069bc

    // ---- UIKit glue (E4 calls these) -------------------------------------------------------------
    bool popoverControllerShouldDismissPopover(EditorPopoverController* popover);      // @ios 100008730  (true)
    void popoverControllerDidDismissPopover(EditorPopoverController* popover);         // @ios 100008738
    // UIAlertViewDelegate alertView:clickedButtonAtIndex: (reads the tag only).
    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;  // @ios 100008768

    // ---- properties -----------------------------------------------------------------------------
    bool pasteInPlace();                                                   // @ios 100008974
    void setPasteInPlace(bool pasteInPlace);                               // @ios 100008984
    const cocos2d::ValueVector& copiedRefs();                              // @ios 100008994
    const cocos2d::Vector<Special*>& lockedRefs();                         // @ios 1000089a4
    cocos2d::Sprite* rotCirc();                                            // @ios 1000089b4
    cocos2d::Sprite* moveCirc();                                           // @ios 1000089c4
    EditorPopoverController* popover();                                                // @ios 1000089d4
    void setPopover(EditorPopoverController* popover);                                 // @ios 1000089e4
    LevelMO* levelMO();                                                    // @ios 1000089f0
    void setLevelMO(LevelMO* levelMO);                                     // @ios 100008a00

    // ---- port helpers ---------------------------------------------------------------------------
    EditorSpriteBatchNode* sbnNode() { return sbn; }
    CCLayerPanZoom* stageNode() { return stage; }
    // iOS point contentSize of a node built from an editor atlas (contentSize * own scale).
    static cocos2d::Size pointSize(cocos2d::Node* node);
    // setScale() for a sprite from an editor atlas placed in point space (this layer's children,
    // the stage): Director content scale / EditorAssets::pixelsPerPoint() (= Special::editorArtScale).
    static float pointSpriteScale();
    // Stage space is in editor-atlas art units (see Special.h "Art scale"): one unit is this many
    // iPad points (= contentScaleFactor / EditorAssets::pixelsPerPoint()).
    static float stageUnitInPoints();
    // The ptm ratio in stage units: kPtmRatio / stageUnitInPoints() (-> Special::setSessionPtmRatio).
    static float stagePtmRatio();
    // The iOS Session levelData dictionary (= LevelSession::getInstance()->levelData(), E5):
    // "data" (level XML), "force_character" (bool), "playable_character" (int).
    static cocos2d::ValueMap& sessionLevelData();

    // ---- EDITOR (browser features, PC addition) ----------------------------------------------
    // The editor on a level given as XML (browser or iOS format) without a LevelMO, e.g. an
    // online level opened for remixing or a file passed on the command line. Saving creates a
    // new user level named `name`.
    static cocos2d::Scene* createSceneWithXML(const std::string& xml, const std::string& name);
    // The iOS editor's own writer (y-up metres, <info v="1.70" ... fm="m">), kept for reference;
    // levelData() writes browser XML now (src/editor/flash/FlashLevelIO.h).
    std::string iosLevelData();
    // flashed::LevelReaderDelegate
    void readerAddCharacter(float xMetres, float yMetres, int character, bool force, bool hideVehicle) override;
    // Inspector (replaces EditParametersView), its functions, link mode, drawing tool, grouping.
    void showInspector();
    void closeInspector();
    void inspectorFunction(const std::string& key);
    void beginLinkMode(TriggerRef* trigger);
    void endLinkMode();
    void beginPolygonTool(bool art);
    void finishPolygonTool(bool keep);
    void groupSelection();
    void ungroupSelection();
    void duplicateSelection();
    void selectAll();
    const std::string& pendingLevelName() const { return _pendingName; }

protected:
    EditorLayer();
    ~EditorLayer() override;

    // EDITOR (browser features, PC addition)
    void showToolBanner(const std::string& text, const std::string& doneText, std::function<void()> done,
                        std::function<void()> cancel);
    void hideToolBanner();
    void installPCInput();
    bool handleKey(cocos2d::EventKeyboard::KeyCode key);
    flashed::Inspector* _inspector = nullptr;
    cocos2d::Node* _palette = nullptr;
    cocos2d::Node* _toolBanner = nullptr;
    TriggerRef* _linkTrigger = nullptr;            // retained through _linkTriggerKeep
    cocos2d::RefPtr<cocos2d::Ref> _linkTriggerKeep;
    bool _polygonTool = false;
    bool _polygonArt = false;
    std::vector<cocos2d::Vec2> _polygonPoints;   // stage space
    cocos2d::DrawNode* _toolDraw = nullptr;
    bool _ctrl = false;
    bool _rightDrag = false;
    cocos2d::Vec2 _rightDragLast;
    cocos2d::EventListener* _keyListener = nullptr;
    cocos2d::EventListener* _mouseListener = nullptr;
    std::string _pendingXML;
    std::string _pendingName;

    void observe(const std::string& name, void* objectFilter, void (EditorLayer::*handler)(void*));
    void removeObserver(const std::string& name);
    void presentAlert(const std::string& title, const std::string& message, const std::string& cancel, int tag);

    // iOS ivars (offsets from the ObjC metadata).
    EditorSpriteBatchNode* sbn = nullptr;            // +0x1c0
    cocos2d::SpriteBatchNode* uiSBN = nullptr;       // +0x1c8  "editorui" atlas, z 101
    CCLayerPanZoom* stage = nullptr;                 // +0x1d0
    bool panStage = false;                           // +0x1d8
    uikit::View* currentUIView = nullptr;            // +0x1e0  the open side panel (E4)
    EditorLayerButton* menuBtn = nullptr;            // +0x1e8  tag 0, top left
    EditorLayerButton* togglePanBtn = nullptr;       // +0x1f0  tag 1, top right
    EditorLayerButton* addBtn = nullptr;             // +0x1f8  tag 2, bottom right
    EditorLayerButton* testLevelBtn = nullptr;       // +0x200  tag 3, right of menu (blue)
    EditorLayerButton* editParamsBtn = nullptr;      // +0x208  tag 4
    EditorLayerButton* trashBtn = nullptr;           // +0x210  tag 5, bottom left
    EditorLayerButton* exitBtn = nullptr;            // +0x218  never created in 1.2.7
    EditorLayerButton* alignBtn = nullptr;           // +0x220  never created in 1.2.7
    EditorLayerButton* undoBtn = nullptr;            // +0x228  tag 7; userObject = its home x
    EditorLayerButton* redoBtn = nullptr;            // +0x230  never created in 1.2.7
    EditorLayerButton* copyBtn = nullptr;            // +0x238  tag 9
    EditorLayerButton* pasteBtn = nullptr;           // +0x240  tag 10
    EditorLayerButton* selectBgBtn = nullptr;        // +0x248  tag 11
    cocos2d::Vector<EditorLayerButton*> uiBtns;      // +0x250
    unsigned int iconTag = 1;                        // +0x258  (tag of a button's icon child)
    cocos2d::Label* _shapesLeftLabel = nullptr;      // +0x260  z 0x236
    cocos2d::Label* _artLeftLabel = nullptr;         // +0x268  z 0x235
    unsigned int shapeCount = 0;                     // +0x270
    unsigned int artCount = 0;                       // +0x274
    uikit::View* testView = nullptr;                 // +0x278  (unused in 1.2.7)
    bool hasSaved = true;                            // +0x280
    LevelMO* _levelMO = nullptr;                     // +0x288  (retained)
    int _bgIndex = 0;                                // +0x290
    unsigned int _bgColor = 0xffffff;                // +0x294
    cocos2d::Label* _rollOverLabel = nullptr;        // +0x298
    cocos2d::Label* _pinchToZoomLabel = nullptr;     // +0x2a0
    EditorLayerButton* _currentBtn = nullptr;        // +0x2a8
    bool _pasteInPlace = false;                      // +0x2b0
    cocos2d::ValueVector _copiedRefs;                // +0x2b8  Special::properties() maps
    cocos2d::Vector<Special*> _lockedRefs;           // +0x2c0
    cocos2d::Sprite* _rotCirc = nullptr;             // +0x2c8  "editorui_rotCirc.png" (in uiSBN)
    cocos2d::Sprite* _moveCirc = nullptr;            // +0x2d0  "editorui_moveCirc.png" (in uiSBN)
    EditorPopoverController* _popover = nullptr;     // +0x2d8  editor menu popover (E4, retained)

    cocos2d::EventListener* _touchListener = nullptr;       // targeted delegate, priority 1
    cocos2d::EventListener* _undoEventListener = nullptr;   // event-group closer, see header
};
