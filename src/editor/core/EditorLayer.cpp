#include "EditorLayer.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>

#include "cocos2d.h"
#include "AddSpecialItemUIView.h"
#include "AlignRefsView.h"
#include "CCLayerPanZoom.h"
#include "CharacterRef.h"
#include "EditParametersView.h"
#include "EditorLayerButton.h"
#include "EditorMenuViewController.h"
#include "EditorSettings.h"
#include "EditorSpriteBatchNode.h"
#include "EditorUndoManager.h"
#include "EditorViewController.h"
#include "Gameplay.h"
#include "GameText.h"
#include "HWWindow.h"
#include "LevelMO.h"
#include "LevelSession.h"
#include "MainMenu.h"
#include "RefShape.h"
#include "SelectBackgroundUIView.h"
#include "Settings.h"
#include "SoundController.h"
#include "Special.h"
#include "UIKitCompat.h"
#include "platform/common/EditorAssets.h"
#include "platform/common/IOSBundle.h"
#include "platform/common/Localization.h"

USING_NS_CC;

const int EditorLayer::kTouchPriority = 3;
const float EditorLayer::kPtmRatio = 72.0f;

namespace {

const char* const kSelRefChange = "sel_ref_change";
const char* const kUndoStackUpdated = "undo_stack_updated";
const char* const kArtCountUpdate = "ART_COUNT_UPDATE";
const char* const kShapeCountUpdate = "SHAPE_COUNT_UPDATE";
const char* const kEditorViewClosed = "editor_view_closed";
const char* const kEditorViewItemAdded = "editor_view_item_added";
const char* const kSaveLevelDone = "save_level_done";
const char* const kMainMenuClosed = "main_menu_closed";

// arm64 fcvtzs (float -> int32): truncation, saturating, NaN -> 0.
int fcvtzs(float v)
{
    if (std::isnan(v))
    {
        return 0;
    }
    if (v >= 2147483648.0f)
    {
        return INT_MAX;
    }
    if (v < -2147483648.0f)
    {
        return INT_MIN;
    }
    return (int)v;
}

// -[NSNumber/NSString floatValue | intValue | boolValue] on a level-data dictionary value
// (raw XML attribute strings or numbers).
float valueFloat(const Value& v)
{
    if (v.getType() == Value::Type::STRING)
    {
        return (float)std::strtod(v.asString().c_str(), nullptr);
    }
    return v.isNull() ? 0.0f : v.asFloat();
}

int valueInt(const Value& v)
{
    if (v.getType() == Value::Type::STRING)
    {
        long long i = std::strtoll(v.asString().c_str(), nullptr, 10);
        return (int)std::max<long long>(INT_MIN, std::min<long long>(INT_MAX, i));
    }
    if (v.getType() == Value::Type::FLOAT || v.getType() == Value::Type::DOUBLE)
    {
        return fcvtzs(v.asFloat());
    }
    return v.isNull() ? 0 : v.asInt();
}

bool valueBool(const Value& v)
{
    if (v.getType() == Value::Type::STRING)
    {
        const char* s = v.asString().c_str();
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        {
            ++s;
        }
        if (*s == 'Y' || *s == 'y' || *s == 'T' || *s == 't')
        {
            return true;
        }
        if (*s == '+' || *s == '-')
        {
            ++s;
        }
        while (*s == '0')
        {
            ++s;
        }
        return *s >= '1' && *s <= '9';
    }
    if (v.getType() == Value::Type::BOOLEAN)
    {
        return v.asBool();
    }
    return v.isNull() ? false : v.asFloat() != 0.0f;
}

Value dictValue(const ValueMap& dict, const std::string& key)
{
    auto it = dict.find(key);
    return it == dict.end() ? Value::Null : it->second;
}

// CCLabelTTF initWithString:fontDefinition: in point space (rendered at design resolution and
// scaled back so it stays sharp inside the point-scaled layer). iOS fonts ArialMT /
// Arial-BoldMT -> the system Arial.
Label* makeLabel(const std::string& text, bool bold, float pointSize, TextHAlignment alignment)
{
    float ptd = EditorAssets::pointsToDesign();
    TTFConfig config;
    Label* label = Label::createWithSystemFont(text, bold ? "Arial Bold" : "Arial", pointSize * ptd,
                                               Size::ZERO, alignment);
    label->setScale(1.0f / ptd);
    return label;
}

Sprite* pointSprite(const std::string& frameName)
{
    Sprite* sprite = Sprite::createWithSpriteFrameName(frameName);
    sprite->setScale(EditorLayer::pointSpriteScale());
    return sprite;
}

EditorLayerButton* pointButton(EditorLayerButton* button)
{
    button->setScale(EditorLayer::pointSpriteScale());
    return button;
}

// [CCSprite textureRect].size in points.
Size textureSizeInPoints(Sprite* sprite)
{
    Size size = sprite->getTextureRect().size;
    return Size(size.width * sprite->getScaleX(), size.height * sprite->getScaleY());
}

Color3B color3B(unsigned int packed)
{
    // ccColor3B passed packed in a register: byte 0 = r.
    return Color3B((GLubyte)(packed & 0xff), (GLubyte)((packed >> 8) & 0xff), (GLubyte)((packed >> 16) & 0xff));
}

}  // namespace

// ---- creation -------------------------------------------------------------------------------------

// @ios 100005c2c
Scene* EditorLayer::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(EditorLayer::create());
    return scene;
}

// @ios 100005bd8
Scene* EditorLayer::createSceneWithLevelMO(LevelMO* levelMO)
{
    Scene* scene = Scene::create();
    scene->addChild(EditorLayer::createWithLevelMO(levelMO));
    return scene;
}

EditorLayer* EditorLayer::create()
{
    EditorLayer* layer = new (std::nothrow) EditorLayer();
    if (layer && layer->init())
    {
        layer->autorelease();
        return layer;
    }
    delete layer;
    return nullptr;
}

EditorLayer* EditorLayer::createWithLevelMO(LevelMO* levelMO)
{
    EditorLayer* layer = new (std::nothrow) EditorLayer();
    if (layer && layer->initWithLevelMO(levelMO))
    {
        layer->autorelease();
        return layer;
    }
    delete layer;
    return nullptr;
}

EditorLayer::EditorLayer()
{
}

// @ios 100005a24 (dealloc)
EditorLayer::~EditorLayer()
{
    uikit::NotificationCenter::removeObserver(this);
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
    }
    if (_undoEventListener)
    {
        _eventDispatcher->removeEventListener(_undoEventListener);
    }
    CC_SAFE_RELEASE(_levelMO);
}

Size EditorLayer::pointSize(Node* node)
{
    const Size& size = node->getContentSize();
    return Size(size.width * node->getScaleX(), size.height * node->getScaleY());
}

float EditorLayer::pointSpriteScale()
{
    return Director::getInstance()->getContentScaleFactor() / EditorAssets::pixelsPerPoint();
}

float EditorLayer::stageUnitInPoints()
{
    return Director::getInstance()->getContentScaleFactor() / EditorAssets::pixelsPerPoint();
}

float EditorLayer::stagePtmRatio()
{
    return kPtmRatio / stageUnitInPoints();
}

ValueMap& EditorLayer::sessionLevelData()
{
    return LevelSession::getInstance()->levelData();
}

// @ios 100004a7c
bool EditorLayer::init()
{
    Settings::getInstance()->getSoundController()->stopBackgroundMusic();
    return initWithLevelMO(nullptr);
}

// @ios 100004ab0
bool EditorLayer::initWithLevelMO(LevelMO* levelMO)
{
    if (!LayerColor::init())
    {
        return false;
    }
    // port: this layer works in iPad points (see header).
    Size winSize = uikit::windowSize();
    setContentSize(winSize);
    setAnchorPoint(Vec2::ZERO);
    setScale(EditorAssets::pointsToDesign());

    iconTag = 1;
    _bgIndex = 0;
    _bgColor = 0xffffff;
    CC_SAFE_RETAIN(levelMO);
    CC_SAFE_RELEASE(_levelMO);
    _levelMO = levelMO;
    _rollOverLabel = nullptr;
    // [[Session sharedSession] setSessionMode:2] -> editor ptmRatio (iPad 72).
    Special::setSessionPtmRatio(stagePtmRatio());
    hasSaved = true;
    setPasteInPlace(false);
    _copiedRefs.clear();
    _lockedRefs.clear();

    double winW = winSize.width;
    double winH = winSize.height;
    addChild(LayerColor::create(Color4B(0x3d, 0x88, 0xc7, 0xff), (float)winW, (float)winH));

    stage = CCLayerPanZoom::create();
    float ptm = stagePtmRatio();
    float unit = stageUnitInPoints();  // port: iOS scales are points per stage point
    double stageHeightPoints = (double)(ptm * 160.0f);
    double minScale = (winH + -60.0) / stageHeightPoints;
    double stageWidthPoints = (double)(ptm * 320.0f);
    stage->setPanBoundsRect(cg::Rect(0.0, 0.0, stageWidthPoints, stageHeightPoints));
    stage->setBorder(30.0f);
    stage->setMinScale((double)(float)minScale);
    stage->setMaxScale(3.0 * (double)unit);
    stage->setScale(0.25f * unit);
    stage->setAnchorPoint(Vec2::ZERO);
    addChild(stage);
    stage->addChild(LayerColor::create(Color4B(0xff, 0xff, 0xff, 0xff), ptm * 320.0f, ptm * 160.0f));

    loadAtlasFiles();
    std::string objectsTexture =
        openwheels::iosBundlePath() + "levelEditorObjects1" + EditorAssets::suffix() + ".png";
    sbn = EditorSpriteBatchNode::create(objectsTexture);
    stage->addChild(sbn, 100);
    DrawNode* drawNode = DrawNode::create();
    stage->addChild(drawNode, 99);
    sbn->setDrawNode(drawNode);
    uiSBN = SpriteBatchNode::create(openwheels::iosBundlePath() + "editorui" + EditorAssets::suffix() + ".png");
    addChild(uiSBN, 0x65);

    float notch = uikit::notchOffset();
    Vector<EditorLayerButton*> buttons;

    menuBtn = pointButton(EditorLayerButton::createWithIcon("editorui_menu.png"));
    menuBtn->setTag(EditorLayerButtonTagMenu);
    Size size = pointSize(menuBtn);
    float topLeftX = (float)((double)notch + size.width * 0.5);
    float topY = (float)(winH + size.height * -0.5);
    menuBtn->setPosition(topLeftX, topY);
    uiSBN->addChild(menuBtn);
    buttons.pushBack(menuBtn);
    menuBtn->createPressState();

    testLevelBtn = pointButton(
        EditorLayerButton::createWithSpriteFrameName("editorui_blueBtn.png", "editorui_test.png"));
    testLevelBtn->setTag(EditorLayerButtonTagTest);
    size = pointSize(testLevelBtn);
    testLevelBtn->setPosition((float)(size.width + (double)topLeftX), topY);
    uiSBN->addChild(testLevelBtn);
    buttons.pushBack(testLevelBtn);
    testLevelBtn->createPressStateWithGrey(false);

    togglePanBtn = pointButton(EditorLayerButton::createWithIcon("editorui_marquee.png"));
    togglePanBtn->setTag(EditorLayerButtonTagTogglePan);
    size = pointSize(togglePanBtn);
    float topRightX = (float)((winW + size.width * -0.5) - (double)notch);
    togglePanBtn->setPosition(topRightX, topY);
    uiSBN->addChild(togglePanBtn);
    buttons.pushBack(togglePanBtn);
    togglePanBtn->createPressState();

    undoBtn = pointButton(EditorLayerButton::createWithIcon("editorui_undo.png"));
    undoBtn->setTag(EditorLayerButtonTagUndo);
    size = pointSize(undoBtn);
    float undoX = (float)((double)topRightX - size.width);
    undoBtn->setPosition(undoX, topY);
    uiSBN->addChild(undoBtn);
    buttons.pushBack(undoBtn);
    undoBtn->setUserObject(Value(undoX));
    disableButton(undoBtn);
    // (sic) iOS disables and creates the press state of redoBtn, which is nil: the undo button
    // never gets a press sprite.
    disableButton(redoBtn);
    if (redoBtn)
    {
        redoBtn->createPressState();
    }

    trashBtn = pointButton(EditorLayerButton::createWithIcon("editorui_trash.png"));
    trashBtn->setTag(EditorLayerButtonTagTrash);
    size = pointSize(trashBtn);
    float bottomY = (float)(size.height * 0.5);
    trashBtn->setPosition((float)((double)notch + size.width * 0.5), bottomY);
    uiSBN->addChild(trashBtn);
    buttons.pushBack(trashBtn);
    trashBtn->createPressState();

    addBtn = pointButton(EditorLayerButton::createWithIcon("editorui_plus.png"));
    addBtn->setTag(EditorLayerButtonTagAdd);
    size = pointSize(addBtn);
    float bottomRightX = (float)((winW + size.width * -0.5) - (double)notch);
    addBtn->setPosition(bottomRightX, bottomY);
    uiSBN->addChild(addBtn);
    buttons.pushBack(addBtn);
    addBtn->createPressState();

    editParamsBtn = pointButton(EditorLayerButton::createWithIcon("editorui_edit.png"));
    editParamsBtn->setTag(EditorLayerButtonTagEditParams);
    size = pointSize(editParamsBtn);
    float x = (float)((double)bottomRightX - size.width);
    editParamsBtn->setPosition(x, bottomY);
    uiSBN->addChild(editParamsBtn);
    buttons.pushBack(editParamsBtn);
    editParamsBtn->createPressState();

    selectBgBtn = pointButton(EditorLayerButton::createWithIcon("editorui_selectBg.png"));
    selectBgBtn->setTag(EditorLayerButtonTagSelectBg);
    size = pointSize(selectBgBtn);
    x = (float)((double)x - size.width);
    selectBgBtn->setPosition(x, bottomY);
    uiSBN->addChild(selectBgBtn);
    buttons.pushBack(selectBgBtn);
    selectBgBtn->createPressState();

    pasteBtn = pointButton(EditorLayerButton::createWithIcon("editorui_paste.png"));
    pasteBtn->setTag(EditorLayerButtonTagPaste);
    size = pointSize(pasteBtn);
    x = (float)((double)x + size.width * -1.5);
    pasteBtn->setPosition(x, bottomY);
    uiSBN->addChild(pasteBtn);
    buttons.pushBack(pasteBtn);
    pasteBtn->createPressState();
    disableButton(pasteBtn);

    copyBtn = pointButton(EditorLayerButton::createWithIcon("editorui_copy.png"));
    copyBtn->setTag(EditorLayerButtonTagCopy);
    size = pointSize(copyBtn);
    copyBtn->setPosition((float)((double)x - size.width), bottomY);
    uiSBN->addChild(copyBtn);
    buttons.pushBack(copyBtn);
    copyBtn->createPressState();

    for (EditorLayerButton* button : buttons)
    {
        button->setOnPressTarget([this](ButtonWithBatchedSprite* b) { handleOnPress(static_cast<EditorLayerButton*>(b)); });
        button->setOnRollOffTarget([this](ButtonWithBatchedSprite* b) { handleOnRollOff(static_cast<EditorLayerButton*>(b)); });
        button->setOnReleaseTarget([this](ButtonWithBatchedSprite* b) { handleOnRelease(static_cast<EditorLayerButton*>(b)); });
    }
    uiBtns = buttons;
    selectionChanged(nullptr);

    _rotCirc = pointSprite("editorui_rotCirc.png");
    _rotCirc->setPosition(Vec2((float)(winW * 0.5), (float)(winH * 0.5)));
    _rotCirc->setOpacity(0x3f);
    uiSBN->addChild(_rotCirc);
    _moveCirc = pointSprite("editorui_moveCirc.png");
    _moveCirc->setPosition(Vec2((float)(winW * 0.5), (float)(winH * 0.5)));
    _moveCirc->setOpacity(0x3f);
    uiSBN->addChild(_moveCirc);
    _moveCirc->setVisible(false);
    _rotCirc->setVisible(false);

    // iPad: counters 16 pt ArialMT, right aligned; 6 / 2 pt spacing.
    double counterOffsetY = 6.0;
    double counterSpacing = 2.0;
    _artLeftLabel = makeLabel(StringUtils::format("%s: %i", Localization::get("ART LEFT").c_str(), 1000),
                              false, 16.0f, TextHAlignment::RIGHT);
    Size addTexture = textureSizeInPoints(addBtn);
    Vec2 addPos = addBtn->getPosition();
    _artLeftLabel->setPosition(Vec2((float)(addTexture.width * 0.5 + ((double)addPos.x + -2.0)),
                                    (float)(addTexture.height * 0.5 + ((double)addPos.y + counterOffsetY))));
    _artLeftLabel->setAnchorPoint(Vec2(1.0f, 0.0f));
    _artLeftLabel->setColor(color3B(0xa6a6a6));
    addChild(_artLeftLabel, 0x235);
    _shapesLeftLabel = makeLabel(StringUtils::format("%s: %i", Localization::get("SHAPES LEFT").c_str(), 600),
                                 false, 16.0f, TextHAlignment::RIGHT);
    Vec2 artPos = _artLeftLabel->getPosition();
    _shapesLeftLabel->setPosition(
        Vec2(artPos.x, (float)((double)artPos.y + counterSpacing + pointSize(_artLeftLabel).height)));
    _shapesLeftLabel->setAnchorPoint(Vec2(1.0f, 0.0f));
    _shapesLeftLabel->setColor(color3B(0xa6a6a6));
    addChild(_shapesLeftLabel, 0x236);

    if (!_levelMO)
    {
        ValueMap character;
        character["x"] = Value("75");
        character["y"] = Value("50");
        character["c"] = Value("2");
        addCharacter(character);
    }
    else
    {
        addLevelItems();
    }
    centerToCharacter();
    stage->setIsTouchEnabled(false);
    panStage = false;

    _pinchToZoomLabel = makeLabel(Localization::get("DRAG SCREEN OR PINCH TO ZOOM"), true, 17.0f,
                                  TextHAlignment::CENTER);
    // y: [_rollOverLabel contentSize].height (nil here -> 0) + 20.
    double rollOverHeight = _rollOverLabel ? pointSize(_rollOverLabel).height : 0.0;
    _pinchToZoomLabel->setPosition(Vec2((float)(winW * 0.5), (float)(rollOverHeight + 20.0)));
    _pinchToZoomLabel->setColor(color3B(0xff));
    addChild(_pinchToZoomLabel);
    _pinchToZoomLabel->setVisible(false);
    showIntroMessage();
    updateShapeCount();
    updateArtCount();
    return true;
}

// @ios 1000058e0
void EditorLayer::loadAtlasFiles()
{
    EditorAssets::loadAtlas("levelEditorObjects1");
    EditorAssets::loadAtlas("editorui");
}

// @ios 100005968
void EditorLayer::addObservers()
{
    observe(kSelRefChange, nullptr, &EditorLayer::selectionChanged);
    observe(kUndoStackUpdated, nullptr, &EditorLayer::undoStackUpdated);
    uikit::NotificationCenter::addObserver(this, kArtCountUpdate, nullptr, [this](void*, void*) { updateArtCount(); });
    uikit::NotificationCenter::addObserver(this, kShapeCountUpdate, nullptr, [this](void*, void*) { updateShapeCount(); });
}

void EditorLayer::observe(const std::string& name, void* objectFilter, void (EditorLayer::*handler)(void*))
{
    uikit::NotificationCenter::addObserver(this, name, objectFilter,
                                           [this, handler](void* object, void*) { (this->*handler)(object); });
}

void EditorLayer::removeObserver(const std::string& name)
{
    uikit::NotificationCenter::removeObserver(this, name, nullptr);
}

void EditorLayer::presentAlert(const std::string& title, const std::string& message, const std::string& cancel,
                               int tag)
{
    // UIAlertView (cancel button only, delegate = self) -> the game's HWWindow.
    HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, this, false, false);
    window->setTag(tag);
    window->showAlertMessage(title, message, cancel, "", true);
}

// @ios 100005ac8
void EditorLayer::showIntroMessage()
{
    UserDefault* defaults = UserDefault::getInstance();
    if (!defaults->getBoolForKey("editor_intro_message_shown"))
    {
        // port: UIAlertView shows above the next scene; HWWindow attaches to the running scene,
        // so wait until this layer is on stage.
        std::string title = Localization::get("LEVEL EDITOR");
        std::string message = Localization::get("EDITOR INTRO MESSAGE");
        std::string ok = Localization::get("OK");
        scheduleOnce([this, title, message, ok](float) { presentAlert(title, message, ok, 0); }, 0.0f,
                     "EditorLayer.introMessage");
    }
    defaults->setBoolForKey("editor_intro_message_shown", true);
    defaults->flush();
}

// @ios 100005c70
void EditorLayer::onEnter()
{
    loadAtlasFiles();
    addObservers();
    Director::getInstance()->startAnimation();
    Special::setSessionPtmRatio(stagePtmRatio());  // [[Session sharedSession] setSessionMode:2]
    // HWTracker setCategory:@"editor" - analytics, dropped on PC.

    // [touchDispatcher addTargetedDelegate:self priority:1 swallowsTouches:YES]
    auto listener = EventListenerTouchOneByOne::create();
    listener->setSwallowTouches(true);
    listener->onTouchBegan = [this](Touch* t, Event* e) { return onTouchBegan(t, e); };
    _eventDispatcher->addEventListenerWithFixedPriority(listener, kTouchPriority);
    _touchListener = listener;

    // port: NSUndoManager groupsByEvent - one touch phase is one run-loop pass on iOS. A
    // non-swallowing one-by-one listener ahead of every other one closes the previous event's
    // undo group before the new phase is handled.
    auto eventGroups = EventListenerTouchOneByOne::create();
    eventGroups->setSwallowTouches(false);
    auto closeGroup = [this]() {
        if (sbn)
        {
            sbn->undoManager()->endEventGroup();
        }
    };
    eventGroups->onTouchBegan = [closeGroup](Touch*, Event*) {
        closeGroup();
        return true;
    };
    eventGroups->onTouchMoved = [closeGroup](Touch*, Event*) { closeGroup(); };
    eventGroups->onTouchEnded = [closeGroup](Touch*, Event*) { closeGroup(); };
    eventGroups->onTouchCancelled = [closeGroup](Touch*, Event*) { closeGroup(); };
    _eventDispatcher->addEventListenerWithFixedPriority(eventGroups, -1000);
    _undoEventListener = eventGroups;

    LayerColor::onEnter();
}

// @ios 100005d14
void EditorLayer::onExit()
{
    uikit::NotificationCenter::removeObserver(this);
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
        _touchListener = nullptr;
    }
    if (_undoEventListener)
    {
        _eventDispatcher->removeEventListener(_undoEventListener);
        _undoEventListener = nullptr;
    }
    if (currentUIView)
    {
        hideMenu(nullptr);
    }
    if (_popover)
    {
        _popover->dismissPopoverAnimated(false);
        _popover->release();
        _popover = nullptr;
    }
    LayerColor::onExit();
}

// ---- loading level data -------------------------------------------------------------------------------

// @ios 100005dc0
void EditorLayer::addLevelItems()
{
    EditorLevelXMLParser parser(_levelMO->data(), this);
    parser.parse();
    sbn->undoManager()->removeAllActions();
    undoStackUpdated(nullptr);
}

// @ios 100005e48
Special* EditorLayer::addSingleItem(int levelItemID, const ValueMap& properties)
{
    Special* ref = EditorSettings::getInstance()->objectForKey((unsigned int)levelItemID);
    if (!ref)
    {
        // iOS: [[nil alloc] init] -> nil, then -[NSMutableArray addObject:nil] throws.
        CCLOG("EditorLayer: no editor ref for level item %d (skipped)", levelItemID);
        return nullptr;
    }
    (void)dynamic_cast<RefShape*>(ref);  // isKindOfClass:[RefShape class] (result unused)
    ref->setProperties(properties);
    ref->createRef();
    sbn->addRef(ref);
    return ref;
}

// @ios 100005ee0
void EditorLayer::addSingleShape(int levelItemID, const ValueMap& properties)
{
}

// @ios 100008770
void EditorLayer::setVersion(float version, unsigned int background, unsigned int color,
                             const ValueVector& parameters)
{
    _bgIndex = (int)background;
    _bgColor = color;
}

// @ios 10000878c
void EditorLayer::addCharacter(const ValueMap& character)
{
    CharacterRef* ref = static_cast<CharacterRef*>(EditorSettings::getInstance()->objectForKey(5000));
    int c = valueInt(dictValue(character, "c"));
    if (c < 2)
    {
        c = 1;
    }
    float x = valueFloat(dictValue(character, "x"));
    float y = valueFloat(dictValue(character, "y"));
    bool forceCharacter = valueBool(dictValue(character, "f"));
    bool hideVehicle = valueBool(dictValue(character, "h"));
    ref->setXMeters(Value(x));
    ref->setYMeters(Value(y));
    ref->setForceCharacter(Value(forceCharacter));
    ref->setHideVehicle(hideVehicle);
    ref->createRef();
    sbn->addRef(ref);
    ref->setDefaultCharacter(Value(c));
}

// @ios 1000088f0
void EditorLayer::addShape(const ValueMap& shape, unsigned int objectIndex)
{
    addSingleItem(valueInt(dictValue(shape, "t")) + 6000, shape);
}

// @ios 100008934
void EditorLayer::addSpecial(const ValueMap& special, unsigned int objectIndex)
{
    addSingleItem(valueInt(dictValue(special, "t")), special);
}

// ---- view ---------------------------------------------------------------------------------------------

// @ios 100005ee4
void EditorLayer::centerToCharacter()
{
    stage->setScale(0.75f * stageUnitInPoints());  // port: stage units (see header)
    centerToRefWithIndex(0, cg::Point(0.25, 0.5));
    sbn->positionMoveRotCircs();
}

// @ios 100005f34
void EditorLayer::centerToRefWithIndex(unsigned int index, const cg::Point& normalPoint)
{
    Size winSize = uikit::windowSize();
    Special* ref = sbn->refs().at(index);
    // [stage convertToWorldSpace:ref.position] in iOS window points (= this layer's space).
    Vec2 world = convertToNodeSpace(stage->convertToWorldSpace(ref->getPosition()));
    Vec2 position = stage->getPosition();
    stage->setPosition(Vec2((float)(((double)winSize.width * normalPoint.x - (double)world.x) + (double)position.x),
                            (float)(((double)winSize.height * normalPoint.y - (double)world.y) + (double)position.y)));
}

// @ios 100006630
void EditorLayer::enable(bool enable)
{
    sbn->setEnableEdit(enable);
    setAllButtonsVisible(enable, Vector<EditorLayerButton*>());
    if (enable)
    {
        refreshButtons();
    }
}

// @ios 100007a58
void EditorLayer::togglePan(EditorLayerButton* sender)
{
    bool wasPanning = panStage;
    bool nowPanning = !wasPanning;
    const char* iconName = wasPanning ? "editorui_marquee.png" : "editorui_pan.png";
    _pinchToZoomLabel->setVisible(nowPanning);
    panStage = nowPanning;
    stage->setIsTouchEnabled(nowPanning);
    sbn->setEnableEdit(wasPanning);
    togglePanBtn->icon()->removeFromParentAndCleanup(false);
    Sprite* icon = Sprite::createWithSpriteFrameName(iconName);
    const Size& buttonSize = togglePanBtn->getContentSize();
    icon->setPosition(Vec2(buttonSize.width * 0.5f, buttonSize.height * 0.5f));
    togglePanBtn->addChild(icon, 1, (int)iconTag);
    togglePanBtn->setIcon(icon);
    _shapesLeftLabel->setVisible(!panStage);
    _artLeftLabel->setVisible(!panStage);
    Vector<EditorLayerButton*> except;
    except.pushBack(togglePanBtn);
    setAllButtonsVisible(!panStage, except);
    if (panStage)
    {
        return;
    }
    selectionChanged(nullptr);
}

// ---- shape / art limits -----------------------------------------------------------------------------------

// @ios 100005ff0
bool EditorLayer::showAlertIfExceedingShapeCount(unsigned int shapeCountToAdd, unsigned int artCountToAdd)
{
    int shapesLeftSigned = kShapeLimit - (int)shapeCount;
    unsigned int shapesLeft = shapesLeftSigned < 1 ? 0u : (unsigned int)shapesLeftSigned;
    int artLeftSigned = kArtLimit - (int)artCount;
    unsigned int artLeft = artLeftSigned < 1 ? 0u : (unsigned int)artLeftSigned;
    bool exceeding = shapesLeft < shapeCountToAdd || artLeft < artCountToAdd;
    if (exceeding)
    {
        const char* artTitle = (artCountToAdd <= artLeft) ? "Shape/Art Limit Reached" : "Art Limit Reached";
        const char* shapeTitle = (artCountToAdd > artLeft || shapeCountToAdd <= shapesLeft)
                                     ? "Shape/Art Limit Reached"
                                     : "Shape Limit Reached";
        const char* title = (shapeCountToAdd <= shapesLeft) ? artTitle : shapeTitle;
        // "You cannot complete this operation. It requires %i shapes and %i art.\n
        //  Shapes left %i. Art left %i" (hard-coded in the iOS binary, not localized)
        const std::string& format = OW_IOSTEXT(editorShapeArtLimitMessage, 0x1015d7e40);
        char message[512];
        std::snprintf(message, sizeof(message), format.c_str(), (int)shapeCountToAdd, (int)artCountToAdd,
                      (int)shapesLeft, (int)artLeft);
        presentAlert(title, message, "Ugh, fine!", 0);
    }
    return !exceeding;
}

// @ios 100006538
bool EditorLayer::canAddItemWithShapeCount(unsigned int shapeCountToAdd, unsigned int artCountToAdd)
{
    int shapesLeftSigned = kShapeLimit - (int)shapeCount;
    unsigned int shapesLeft = shapesLeftSigned < 1 ? 0u : (unsigned int)shapesLeftSigned;
    int artLeftSigned = kArtLimit - (int)artCount;
    unsigned int artLeft = artLeftSigned < 1 ? 0u : (unsigned int)artLeftSigned;
    return shapeCountToAdd <= shapesLeft && artCountToAdd <= artLeft;
}

// @ios 100006358
void EditorLayer::updateArtCount()
{
    int total = 0;
    for (ssize_t i = 0; i < sbn->refs().size(); i++)
    {
        total = (int)sbn->refs().at(i)->artCount() + total;
    }
    artCount = (unsigned int)total;
    int left = kArtLimit - total;
    if (_artLeftLabel)  // nil until init creates it
        _artLeftLabel->setString(
            StringUtils::format("%s: %i", Localization::get("ART LEFT").c_str(), left > 0 ? left : 0));
}

// @ios 100006448
void EditorLayer::updateShapeCount()
{
    int total = 0;
    for (ssize_t i = 0; i < sbn->refs().size(); i++)
    {
        total = (int)sbn->refs().at(i)->shapeCount() + total;
    }
    shapeCount = (unsigned int)total;
    int left = kShapeLimit - total;
    if (_shapesLeftLabel)  // nil until init creates it
        _shapesLeftLabel->setString(
            StringUtils::format("%s: %i", Localization::get("SHAPES LEFT").c_str(), left > 0 ? left : 0));
}

// ---- toolbar -----------------------------------------------------------------------------------------------

// @ios 1000060f4
void EditorLayer::addItemsBtnPressed(EditorLayerButton* sender)
{
    Size winSize = uikit::windowSize();
    double width = (double)(long long)std::fmin((double)winSize.width * 0.5, 284.0);
    float notch = uikit::notchOffset();
    Rect frame((float)((double)winSize.width - ((double)notch + width)), 0.0f, (float)width, winSize.height);
    AddSpecialItemUIView* view = AddSpecialItemUIView::create(frame);
    uikit::window()->addSubview(view);
    currentUIView = view;
    observe(kEditorViewClosed, view, &EditorLayer::hideMenu);
    observe(kEditorViewItemAdded, nullptr, &EditorLayer::handleMenuTouch);
}

// @ios 100006214
bool EditorLayer::onTouchBegan(Touch* touch, Event* event)
{
    return false;
}

// @ios 10000621c
void EditorLayer::handleMenuTouch(void* notification)
{
    removeObserver(kEditorViewItemAdded);
    int levelItemID = notification ? *static_cast<int*>(notification) : 0;
    Size winSize = uikit::windowSize();
    Vec2 center = sbn->convertToNodeSpace(
        convertToWorldSpace(Vec2((float)((double)winSize.width * 0.5), (float)((double)winSize.height * 0.5))));
    Special* ref = EditorSettings::getInstance()->objectForKey((unsigned int)levelItemID);
    if (!ref)
    {
        CCLOG("EditorLayer: no editor ref for level item %d", levelItemID);
        return;
    }
    if (showAlertIfExceedingShapeCount(ref->shapeCount(), ref->artCount()))
    {
        ref->setPosition(center.x, center.y);
        ref->createRef();
        Vector<Special*> refs;
        refs.pushBack(ref);
        sbn->addRefs(refs);
    }
}

// @ios 100006354
void EditorLayer::selectionChanged(void* notification)
{
    refreshButtons();
}

// @ios 100006580
void EditorLayer::undoStackUpdated(void* notification)
{
    EditorUndoManager* undoManager = sbn->undoManager();
    if (!undoManager->canUndo())
    {
        disableButton(undoBtn);
    }
    else
    {
        hasSaved = false;
        enableButton(undoBtn);
    }
    if (!undoManager->canRedo())
    {
        disableButton(redoBtn);
        return;
    }
    enableButton(redoBtn);
}

// @ios 100006a20
void EditorLayer::refreshButtons()
{
    auto setEnabled = [](EditorLayerButton* button, bool enabled) {
        if (button)
        {
            button->setIsEnabled(enabled);
        }
    };
    ssize_t count = sbn->selectedRefs().size();
    if ((int)count == 1)
    {
        bool notCharacter = dynamic_cast<CharacterRef*>(sbn->selectedRefs().at(0)) == nullptr;
        setEnabled(trashBtn, notCharacter);
        setEnabled(copyBtn, notCharacter);
        setEnabled(alignBtn, false);
        setEnabled(editParamsBtn, true);
    }
    else if ((int)count == 0)
    {
        setEnabled(editParamsBtn, false);
        setEnabled(alignBtn, false);
        setEnabled(copyBtn, false);
        setEnabled(trashBtn, false);
    }
    else
    {
        setEnabled(alignBtn, true);
        setEnabled(copyBtn, true);
        setEnabled(trashBtn, true);
        setEnabled(editParamsBtn, false);
    }
    // (iOS first runs this from init, before the counter labels exist: messages to nil.)
    if (!currentUIView && _artLeftLabel && _shapesLeftLabel)
    {
        _artLeftLabel->setVisible(true);
        _shapesLeftLabel->setVisible(true);
    }
    setEnabled(undoBtn, canUndo());
    setEnabled(pasteBtn, !_copiedRefs.empty());
}

// @ios 100006bec
void EditorLayer::setAllButtonsVisible(bool visible, const Vector<EditorLayerButton*>& exceptThese)
{
    for (EditorLayerButton* button : uiBtns)
    {
        if (exceptThese.contains(button))
        {
            continue;
        }
        button->setVisible(visible);
        button->setIsEnabled(visible);
    }
}

// @ios 100006d2c
void EditorLayer::disableButton(EditorLayerButton* button)
{
    if (!button)
    {
        return;  // messages to nil (redoBtn)
    }
    Node* icon = button->getChildByTag((int)iconTag);
    if (icon)
    {
        icon->setOpacity(0x59);
    }
    button->setOpacity(0x3f);
    button->setIsEnabled(false);
}

// @ios 100006d78
void EditorLayer::enableButton(EditorLayerButton* button)
{
    if (!button)
    {
        return;
    }
    Node* icon = button->getChildByTag((int)iconTag);
    if (icon)
    {
        icon->setOpacity(0xff);
    }
    button->setOpacity(0xff);
    button->setIsEnabled(true);
}

// @ios 1000072f0
void EditorLayer::undoBtnPressed(EditorLayerButton* sender)
{
    undo();
}

// @ios 1000072f4
void EditorLayer::redoBtnPressed(EditorLayerButton* sender)
{
    redo();
}

// @ios 1000072f8
void EditorLayer::copyBtnPressed(EditorLayerButton* sender)
{
    copySelection();
}

// @ios 1000072fc
void EditorLayer::pasteBtnPressed(EditorLayerButton* sender)
{
    pasteInPlace(pasteInPlace());
}

// @ios 100007420
void EditorLayer::handleOnPress(EditorLayerButton* sender)
{
    if (_currentBtn)
    {
        return;
    }
    _currentBtn = sender;
    // +[ButtonFactory localizedStringActionForEditorButtonType:] = "EDITORBTN %i"
    std::string text = Localization::get(StringUtils::format("EDITORBTN %i", sender->getTag()));
    Size winSize = uikit::windowSize();
    _rollOverLabel = makeLabel(text, true, 17.0f, TextHAlignment::CENTER);
    _rollOverLabel->setPosition(Vec2((float)((double)winSize.width * 0.5),
                                     (float)(((double)winSize.height + -5.0) - pointSize(_rollOverLabel).height)));
    _rollOverLabel->setColor(color3B(0xc68a40));
    addChild(_rollOverLabel);
}

// @ios 100007560
void EditorLayer::handleOnRollOff(EditorLayerButton* sender)
{
    if (_currentBtn != sender)
    {
        return;
    }
    _currentBtn = nullptr;
    removeRollOverLabel();
}

// @ios 100007580
void EditorLayer::handleOnRelease(EditorLayerButton* sender)
{
    if (_currentBtn != sender)
    {
        return;
    }
    _currentBtn = nullptr;
    removeRollOverLabel();
    switch (sender->getTag())
    {
    case EditorLayerButtonTagMenu:
        showMainMenuBtnPressed(sender);
        return;
    case EditorLayerButtonTagTogglePan:
        togglePan(sender);
        return;
    case EditorLayerButtonTagAdd:
        addItemsBtnPressed(sender);
        return;
    case EditorLayerButtonTagTest:
        testLevel(sender);
        return;
    case EditorLayerButtonTagEditParams:
        editParamsBtnPressed(sender);
        return;
    case EditorLayerButtonTagTrash:
        trashBtnPressed(sender);
        return;
    case EditorLayerButtonTagAlign:
        alignBtnPressed(sender);
        return;
    case EditorLayerButtonTagUndo:
        undoBtnPressed(sender);
        return;
    case EditorLayerButtonTagRedo:
        redoBtnPressed(sender);
        return;
    case EditorLayerButtonTagCopy:
        copyBtnPressed(sender);
        return;
    case EditorLayerButtonTagPaste:
        pasteBtnPressed(sender);
        return;
    case EditorLayerButtonTagSelectBg:
        selectBgBtnPressed(sender);
        return;
    case EditorLayerButtonTagExit:
        exitEditor(sender);
        return;
    default:
        return;
    }
}

// @ios 1000076e8
void EditorLayer::removeRollOverLabel()
{
    if (_rollOverLabel)
    {
        _rollOverLabel->removeFromParentAndCleanup(false);
        _rollOverLabel = nullptr;
    }
}

// @ios 100007720
void EditorLayer::showMainMenuBtnPressed(EditorLayerButton* sender)
{
    observe(kSaveLevelDone, nullptr, &EditorLayer::saveLevelComplete);
    observe(kMainMenuClosed, nullptr, &EditorLayer::removeMainMenu);
    applyLevelDataToSession();
    EditorMenuViewController* menu = EditorMenuViewController::create("EditorMenuViewController", _levelMO);
    menu->setEditorLayer(this);
    // iPad branch (userInterfaceIdiom == 1): a 480 x 320 popover from the menu button.
    EditorPopoverController* popover = EditorPopoverController::create(menu);
    setPopover(popover);
    popover->shouldDismissPopover = [this](EditorPopoverController* p) {
        return popoverControllerShouldDismissPopover(p);
    };
    popover->didDismissPopover = [this](EditorPopoverController* p) { popoverControllerDidDismissPopover(p); };
    popover->setPopoverContentSize(Size(480.0f, 320.0f));
    menu->setPopoverController(popover);
    Size winSize = uikit::windowSize();
    Vec2 menuPos = menuBtn->getPosition();
    // presentPopoverFromRect:CGRectMake(menu.x, winH - menu.y, 1, 1) inView:director.view
    //   permittedArrowDirections:UIPopoverArrowDirectionAny animated:NO
    popover->presentPopoverFromRect(
        Rect((float)(double)menuPos.x, (float)((double)winSize.height - (double)menuPos.y), 1.0f, 1.0f), 0xf, false);
}

// @ios 10000791c
void EditorLayer::removeMainMenu(void* notification)
{
    removeObserver(kSaveLevelDone);
    removeObserver(kMainMenuClosed);
    if (_popover)
    {
        _popover->dismissPopoverAnimated(true);
        setPopover(nullptr);
    }
    EditorViewController::dismissRootPresented(true, nullptr);
}

// @ios 1000079b4
void EditorLayer::hideMenu(void* notification)
{
    sbn->setEnableEdit(true);
    removeObserver(kEditorViewClosed);
    if (currentUIView)
    {
        currentUIView->removeFromSuperview();
    }
    currentUIView = nullptr;
    float homeX = undoBtn->userObject().asFloat();
    undoBtn->setPosition(homeX, undoBtn->getPosition().y);
    enable(true);
}

// @ios 100007bfc
void EditorLayer::alignBtnPressed(EditorLayerButton* sender)
{
    if (sbn->selectedRefs().size() < 2)
    {
        return;
    }
    Size winSize = uikit::windowSize();
    double half = (double)winSize.width * 0.5;
    float notch = uikit::notchOffset();
    Rect frame((float)(half - (double)notch), 0.0f, (float)half, winSize.height);
    AlignRefsView* view = AlignRefsView::create(frame, sbn->selectedRefs(), sbn);
    currentUIView = view;
    observe(kEditorViewClosed, view, &EditorLayer::hideMenu);
    uikit::window()->addSubview(view);
}

// @ios 100007d24
void EditorLayer::trashBtnPressed(EditorLayerButton* sender)
{
    sbn->deleteSelectedRefs();
    updateArtCount();
    updateShapeCount();
}

// @ios 100007d5c
void EditorLayer::selectBgBtnPressed(EditorLayerButton* sender)
{
    Size winSize = uikit::windowSize();
    double width = (double)(long long)std::fmin((double)winSize.width * 0.5, 284.0);
    float notch = uikit::notchOffset();
    Rect frame((float)((double)winSize.width - ((double)notch + width)), 0.0f, (float)width, winSize.height);
    SelectBackgroundUIView* view = SelectBackgroundUIView::create(frame, _bgIndex, _bgColor);
    view->setDelegate(this);
    uikit::window()->addSubview(view);
    currentUIView = view;
    observe(kEditorViewClosed, view, &EditorLayer::hideMenu);
}

// @ios 100007e74
void EditorLayer::editorUIView(EditorUIView* view, const Value& value, const std::string& key,
                               const ValueMap& userInfo)
{
    if (key == "bg")
    {
        int bg = valueInt(value);
        _bgIndex = bg;
        if (bg == 0)
        {
            _bgColor = 0xffffff;
            return;
        }
        if (bg != -1)
        {
            return;
        }
        _bgIndex = 0;
        Value color = dictValue(userInfo, "color");  // nil userInfo -> empty map
        _bgColor = (unsigned int)valueInt(color);  // -integerValue (truncated to 32 bits)
        return;
    }
    if (key == "bgColor")
    {
        _bgColor = (unsigned int)valueInt(value);
    }
}

// @ios 100007f24
void EditorLayer::editParamsBtnPressed(EditorLayerButton* sender)
{
    Vector<EditorLayerButton*> except;
    except.pushBack(undoBtn);
    setAllButtonsVisible(false, except);
    _artLeftLabel->setVisible(false);
    _shapesLeftLabel->setVisible(false);
    Size winSize = uikit::windowSize();
    double width = (double)(long long)std::fmin((double)winSize.width * 0.5, 284.0);
    double left = (double)winSize.width - width;
    float notch = uikit::notchOffset();
    Rect frame((float)(left - (double)notch), 0.0f, (float)width, winSize.height);
    EditParametersView* view = EditParametersView::create(frame, sbn->selectedRefs());
    view->setUndoManager(sbn->undoManager());
    currentUIView = view;
    uikit::window()->addSubview(view);
    Size undoSize = pointSize(undoBtn);
    notch = uikit::notchOffset();
    undoBtn->setPosition((float)((left + undoSize.width * -0.5) - (double)notch), undoBtn->getPosition().y);
    observe(kEditorViewClosed, nullptr, &EditorLayer::hideMenu);
}

// @ios 1000080d4
void EditorLayer::paramChanged(void* notification)
{
    sbn->updateSelectionRect();
}

// ---- editing operations ----------------------------------------------------------------------------------

// @ios 100006684
void EditorLayer::lockSelection()
{
    Vector<Special*> selection = sbn->selectedRefs();
    for (Special* ref : selection)
    {
        if (dynamic_cast<CharacterRef*>(ref) == nullptr)
        {
            ref->setLocked(true);
            _lockedRefs.pushBack(ref);
        }
    }
    sbn->setSelectedRefs(Vector<Special*>());
}

// @ios 1000067d4
void EditorLayer::unlockAll()
{
    for (Special* ref : _lockedRefs)
    {
        ref->setLocked(false);
    }
    _lockedRefs.clear();
}

// @ios 1000068e0
void EditorLayer::setSnapToAngle(bool snapToAngle)
{
    sbn->setSnapToAngle(snapToAngle);
}

// @ios 1000068f0
bool EditorLayer::snapToAngle()
{
    return sbn->snapToAngle();
}

// @ios 100006900
void EditorLayer::setRotateItemsIndependently(bool rotateIndependently)
{
    sbn->setRotateRefsIndependently(rotateIndependently);
}

// @ios 100006910
bool EditorLayer::rotateItemsIndependently()
{
    return sbn->rotateRefsIndependently();
}

// @ios 100006920
void EditorLayer::setLockToAxis(bool lockToAxis)
{
    sbn->setLockToAxis(lockToAxis);
}

// @ios 100006930
bool EditorLayer::lockToAxis()
{
    return sbn->lockToAxis();
}

// @ios 100006940
bool EditorLayer::unsavedChanges()
{
    if (!canUndo())
    {
        return false;
    }
    return !hasSaved;
}

// @ios 100006978
const Vector<Special*>& EditorLayer::selectedRefs()
{
    return sbn->selectedRefs();
}

// @ios 100006dc4
void EditorLayer::copySelection()
{
    _copiedRefs.clear();
    for (Special* ref : sbn->selectedRefs())
    {
        if (dynamic_cast<CharacterRef*>(ref) == nullptr)
        {
            _copiedRefs.push_back(Value(ref->properties()));
        }
    }
    enableButton(pasteBtn);
}

// @ios 100006f24
void EditorLayer::pasteInPlace(bool inPlace)
{
    Vector<Special*> pasted;
    for (size_t i = 0; i < _copiedRefs.size(); i++)
    {
        const ValueMap& properties = _copiedRefs[i].asValueMap();
        int levelItemID = valueInt(dictValue(properties, "t"));
        Special* ref = EditorSettings::getInstance()->objectForKey((unsigned int)levelItemID);
        if (!ref)
        {
            continue;
        }
        ref->setProperties(properties);
        ref->createRef();
        pasted.pushBack(ref);
    }
    unsigned int shapes = 0;
    unsigned int art = 0;
    for (Special* ref : pasted)
    {
        shapes = ref->shapeCount() + shapes;
        art = ref->artCount() + art;
    }
    if (showAlertIfExceedingShapeCount(shapes, art))
    {
        sbn->addRefs(pasted);
        if (!inPlace)
        {
            centerSelected();
            sbn->updateSelectionRect();
            sbn->positionMoveRotCircs();
        }
        updateShapeCount();
        updateArtCount();
    }
}

// @ios 100007150
void EditorLayer::centerSelected()
{
    const Vector<Special*>& selection = sbn->selectedRefs();
    if (selection.empty())
    {
        return;
    }
    cg::Rect bounds(selection.at(0)->refBoundingBox());
    for (ssize_t i = 1; i < selection.size(); i++)
    {
        bounds = cg::rectUnion(bounds, cg::Rect(selection.at(i)->refBoundingBox()));
    }
    Size winSize = uikit::windowSize();
    Vec2 center = sbn->convertToNodeSpace(
        convertToWorldSpace(Vec2((float)((double)winSize.width * 0.5), (float)((double)winSize.height * 0.5))));
    float offsetY = (float)((bounds.size.height * -0.5 - bounds.origin.y) + (double)center.y);
    float offsetX = (float)((bounds.size.width * -0.5 - bounds.origin.x) + (double)center.x);
    for (ssize_t i = 0; i < selection.size(); i++)
    {
        Special* ref = selection.at(i);
        Vec2 pos = ref->getPosition();
        ref->setPosition((float)((double)pos.x + (double)offsetX), (float)((double)pos.y + (double)offsetY));
    }
}

// @ios 100007324
bool EditorLayer::canUndo()
{
    return sbn->undoManager()->canUndo();
}

// @ios 100007344
void EditorLayer::undo()
{
    sbn->undo();
    updateShapeCount();
    updateArtCount();
}

// @ios 10000737c
void EditorLayer::redo()
{
    sbn->redo();
    updateShapeCount();
    updateArtCount();
}

// @ios 1000073b4
void EditorLayer::reset()
{
    disableButton(undoBtn);
    disableButton(redoBtn);
    _copiedRefs.clear();
    _lockedRefs.clear();
    sbn->reset();
}

// ---- level data / persistence / test play -------------------------------------------------------------

// @ios 1000080e4
unsigned int EditorLayer::characterIndex()
{
    return (unsigned int)static_cast<CharacterRef*>(sbn->refs().at(0))->defaultCharacter();
}

// @ios 10000810c
bool EditorLayer::forceCharacter()
{
    return static_cast<CharacterRef*>(sbn->refs().at(0))->forceCharacter() != 0;
}

// Appends one "p%i" attribute exactly like -[EditorLayer levelData].
static void appendPropertyAttribute(std::string& xml, int index, float value)
{
    char buffer[64];
    if (std::fmod(value, 1.0f) == 0.0f)
    {
        std::snprintf(buffer, sizeof(buffer), "p%i=\"%i\" ", index, fcvtzs(value));
    }
    else
    {
        std::snprintf(buffer, sizeof(buffer), "p%i=\"%.02f\" ", index, (double)value);
    }
    xml += buffer;
}

static void appendProperties(std::string& xml, Special* ref)
{
    std::vector<std::string> keys = ref->propertyKeys();
    for (size_t k = 0; k < keys.size(); k++)
    {
        Value v = ref->valueForKey(keys[k]);
        float value;
        if (v.getType() == Value::Type::BOOLEAN)
        {
            value = v.asBool() ? 1.0f : 0.0f;
        }
        else
        {
            value = valueFloat(v);
        }
        appendPropertyAttribute(xml, (int)k, value);
    }
}

// @ios 100008140
std::string EditorLayer::levelData()
{
    CharacterRef* character = static_cast<CharacterRef*>(sbn->refs().at(0));
    char info[512];
    std::snprintf(info, sizeof(info),
                  "<levelXML><info v=\"%.02f\" x=\"%.02f\" y=\"%.02f\" c=\"%i\" f=\"%i\" h=\"%i\" bg=\"%i\" "
                  "bgc=\"%i\" e=\"1\" fm=\"m\"/>",
                  (double)1.7f, (double)character->xMeters(), (double)character->yMeters(),
                  character->defaultCharacter(), (int)character->forceCharacter(), (int)character->hideVehicle(),
                  _bgIndex, (int)_bgColor);
    std::string xml = info;

    Vector<Special*> specials;
    Vector<Special*> shapes;
    const Vector<Special*>& refs = sbn->refs();
    if (refs.size() > 1)
    {
        for (ssize_t i = 1; i < refs.size(); i++)
        {
            Special* ref = refs.at(i);
            if (dynamic_cast<RefShape*>(ref))
            {
                shapes.pushBack(ref);
            }
            else
            {
                specials.pushBack(ref);
            }
        }
    }
    if (!shapes.empty())
    {
        xml += "<shapes>";
        for (Special* shape : shapes)
        {
            char head[64];
            std::snprintf(head, sizeof(head), "<sh t=\"%i\" i=\"%i\" ", shape->levelItemID() + -6000,
                          (int)shape->interactive());
            xml += head;
            appendProperties(xml, shape);
            xml += " />";
        }
        xml += "</shapes>";
    }
    if (!specials.empty())
    {
        xml += "<specials>";
        for (Special* special : specials)
        {
            char head[32];
            std::snprintf(head, sizeof(head), "<sp t=\"%i\" ", special->levelItemID());
            xml += head;
            appendProperties(xml, special);
            xml += " />";
        }
        xml += "</specials>";
    }
    xml += "</levelXML>";
    return xml;
}

// @ios 1000085b8
void EditorLayer::applyLevelDataToSession()
{
    LevelSession* session = LevelSession::getInstance();
    CharacterRef* character = static_cast<CharacterRef*>(sbn->refs().at(0));
    ValueMap& levelData = session->levelData();
    levelData["data"] = Value(this->levelData());
    levelData["force_character"] = Value(character->forceCharacter() != 0);
    levelData["playable_character"] = Value(character->defaultCharacter());
    // -[Session setCharacterIndex:] takes the character id (it matches characters[i]["id"]).
    session->setCharacterIndex(character->defaultCharacter());
    session->setVehicleIndex(0);
    // port: the Android game reads the character from Settings (by id); iOS keeps the editor's
    // character selected after testing, so this is not restored.
    Settings::getInstance()->setSelectedCharacterId(character->defaultCharacter());
}

// @ios 1000086b4
void EditorLayer::testLevel(EditorLayerButton* sender)
{
    applyLevelDataToSession();
    std::string xml = sessionLevelData()["data"].asString();
    Director::getInstance()->pushScene(Gameplay::createTestingScene(xml));
}

// @ios 1000086f4
void EditorLayer::exitEditor(EditorLayerButton* sender)
{
    Director::getInstance()->replaceScene(MainMenu::createScene(MenuModeMain, nullptr));
}

// @ios 100006988
void EditorLayer::saveLevelComplete(void* notification)
{
    hasSaved = true;
    EditorViewController::dismissRootPresented(true, nullptr);
}

// @ios 1000069bc
void EditorLayer::newLevel()
{
    CC_SAFE_RELEASE(_levelMO);
    _levelMO = nullptr;
    hasSaved = true;
    sbn->reset();
    centerToCharacter();
    updateShapeCount();
    updateArtCount();
}

// ---- UIKit glue -------------------------------------------------------------------------------------------

// @ios 100008730
bool EditorLayer::popoverControllerShouldDismissPopover(EditorPopoverController* popover)
{
    return true;
}

// @ios 100008738
void EditorLayer::popoverControllerDidDismissPopover(EditorPopoverController* popover)
{
    setPopover(nullptr);
}

// @ios 100008768
void EditorLayer::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    (void)window->getTag();  // alertView.tag is read and ignored
}

// ---- properties ---------------------------------------------------------------------------------------------

// @ios 100008974
bool EditorLayer::pasteInPlace()
{
    return _pasteInPlace;
}

// @ios 100008984
void EditorLayer::setPasteInPlace(bool pasteInPlace)
{
    _pasteInPlace = pasteInPlace;
}

// @ios 100008994
const ValueVector& EditorLayer::copiedRefs()
{
    return _copiedRefs;
}

// @ios 1000089a4
const Vector<Special*>& EditorLayer::lockedRefs()
{
    return _lockedRefs;
}

// @ios 1000089b4
Sprite* EditorLayer::rotCirc()
{
    return _rotCirc;
}

// @ios 1000089c4
Sprite* EditorLayer::moveCirc()
{
    return _moveCirc;
}

// @ios 1000089d4
EditorPopoverController* EditorLayer::popover()
{
    return _popover;
}

// @ios 1000089e4
void EditorLayer::setPopover(EditorPopoverController* popover)
{
    CC_SAFE_RETAIN(popover);
    CC_SAFE_RELEASE(_popover);
    _popover = popover;
}

// @ios 1000089f0
LevelMO* EditorLayer::levelMO()
{
    return _levelMO;
}

// @ios 100008a00
void EditorLayer::setLevelMO(LevelMO* levelMO)
{
    CC_SAFE_RETAIN(levelMO);
    CC_SAFE_RELEASE(_levelMO);
    _levelMO = levelMO;
}
