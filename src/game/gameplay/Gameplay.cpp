#include "Gameplay.h"

#include <new>
#include <vector>

#include "AdController.h"
#include "BackgroundLayer.h"
#include "CharacterB2D.h"
#include "CharacterSelectLayer.h"
#include "DebugLayer.h"
#include "DyingVignette.h"
#include "GameText.h"
#include "GameplayControls.h"
#include "GameplayTimer.h"
#include "Globals.h"
#include "HWWindow.h"
#include "HighlightSprite.h"
#include "LevelB2D.h"
#include "MainMenu.h"
#include "Patch.h"
#include "PauseLayer.h"
#include "ReplayData.h"
#include "ResetWorkaroundScene.h"
#include "Session.h"
#include "Settings.h"
#include "SoundController.h"
#include "StageCamera.h"
#include "Tracker.h"
#include "VictoryMenu.h"
#include "LevelSession.h"  // EDITOR (iOS port): user levels (src/editor/persistence)
#include "online/FlashRuntime.h"           // ONLINE (PC addition)
#include "online/vehicles/UserVehicle.h"  // ONLINE (PC addition)
#include "online/replays/ReplayRuntime.h"  // ONLINE (PC addition)
#include "net/race/RaceHooks.h"  // NET (PC addition): ghost race

USING_NS_CC;

// _INIT_7 (listed under HWWindowDelegate in the function index)
static std::string s_trackerCategory = "level";  // @00ac6170
// displayInternalAd: the first call shows the "please turn on data" window instead of the ad.
static bool s_pleaseTurnOnDataShown = false;     // @00ac6188

// @005b900c
Gameplay::Gameplay()
    : _gameplayStarted(false),
      _exitTransitionStarted(false),
      _paused(false),
      _pauseLayer(nullptr),
      _controls(nullptr),
      _timer(nullptr),
      _session(nullptr),
      _levelXml(),
      _isTesting(false),
      _backgroundLayer(nullptr),
      _replayData(nullptr),
      _gameplayMenuActionListener(nullptr),
      _unk0x3b8(nullptr),
      _gameplayControlsActionListener(nullptr),
      _levelCompletedListener(nullptr),
      _unk0x3e8(nullptr),
      _debugLayer(nullptr),
      _unk0x3f8(nullptr),
      _unk0x400(nullptr),
      _unk0x408(nullptr),
      _unk0x410(nullptr),
      _unk0x418(nullptr),
      _unk0x420(nullptr),
      _unk0x430(nullptr)
{
    // Not initialised (as in the original): _internalAdNode/_internalAdLabel/_internalAdCountdown,
    // the dying/dead/ejected listeners, _unk0x428, _level and +0x460..+0x480.
    _levelXml = "";
    _victoryMenu = nullptr;
    _dyingVignette = nullptr;
    _characterDroppedOffscreen = false;
    _levelComplete = false;
    _isReplay = false;
    _characterDead = false;
    _systemTriggerListener = nullptr;
    _highlightNode = nullptr;
}

// @005b8e44 (D1), @005b8edc (D0)
Gameplay::~Gameplay()
{
    Settings::getInstance()->getAdController()->endGameplayTimer();
    if (_replayData && !_isReplay)
    {
        delete _replayData;
        _replayData = nullptr;
        _internalAdNode = nullptr;
        _continueBtnParent = nullptr;
        _unk0x460 = nullptr;
        _pleaseTurnOnDataWindow = nullptr;
        _internalAdBtn = nullptr;
        _internalAdMenu = nullptr;
    }
}

// @005b8f00
Gameplay* Gameplay::create(std::string levelXml)
{
    Gameplay* gameplay = new (std::nothrow) Gameplay();
    if (gameplay)
    {
        if (gameplay->init(levelXml))
        {
            gameplay->autorelease();
        }
        else
        {
            delete gameplay;
            gameplay = nullptr;
        }
    }
    return gameplay;
}

// @005b90e0
Scene* Gameplay::createScene(std::string levelXml, ReplayData* replayData)
{
    Scene* scene = Scene::create();
    Gameplay* layer = Gameplay::create(levelXml);
    layer->setReplayData(replayData);
    scene->addChild(layer);
    return scene;
}

// EDITOR (iOS port): +[GameplayLayer testingScene] @ios 100045408
Scene* Gameplay::createTestingScene(std::string levelXml)
{
    Scene* scene = Scene::create();
    Gameplay* layer = Gameplay::create(levelXml);
    layer->setIsTesting(true);
    scene->addChild(layer, 1);
    return scene;
}

// EDITOR (iOS port): -[GameplayLayer setIsTesting:] @ios 100048af0
void Gameplay::setIsTesting(bool isTesting)
{
    _isTesting = isTesting;
}

// EDITOR (iOS port): -[GameplayLayer isTesting] @ios 100048adc
bool Gameplay::isTesting()
{
    return _isTesting;
}

// @005b91e4
void Gameplay::setReplayData(ReplayData* replayData)
{
    if (replayData)
    {
        _replayData = replayData;
        _isReplay = true;
    }
    if (_controls && _isReplay)
    {
        _controls->setMode(ControlsModeReplay);
    }
}

// @005b9228
void Gameplay::beginGameplayFollowingInterstitial()
{
    if (_gameplayStarted)
    {
        return;
    }
    _gameplayStarted = true;
    _exitTransitionStarted = false;
    Settings::getInstance()->getSoundController()->setPaused(false);
    if (!_isReplay)
    {
        _replayData = new ReplayData();
    }

    Settings* settings = Settings::getInstance();
    settings->getSoundController()->stopAllSounds(false);
    _session = Session::create(1.0f, settings->getSoundController(), SessionModeGameplay);
    settings->setCurrentSession(_session);
    addChild(_session, 1);
    _session->createWorld();
    _backgroundLayer = BackgroundLayer::create();
    _session->setBackgroundLayer(_backgroundLayer);
    addChild(_backgroundLayer, 0);
    _session->setupLevel(_levelXml, false);
    _level = _session->getLevel();

    _controls = GameplayControls::createWithControlsType(
        (ControlsType)settings->getSelectedCharacterControlType());
    // EDITOR (iOS port): -[GameplayLayer addControls] passes mode 1 when testing.
    if (_isTesting)
    {
        _controls->setMode(ControlsModeTesting);
    }
    _session->setControls(_controls);
    addChild(_controls, 4);
    Size winSize = Director::getInstance()->getWinSize();  // unused
    _timer = GameplayTimer::create();
    _timer->setBackgroundSprite(_controls->getTimerBg());
    addChild(_timer, 6);

    addListeners();
    scheduleUpdate();
    settings->getAdController()->startGameplayTimer();
    if (settings->getShouldDisplayPersistentBannerDuringGameplay())
    {
        settings->getAdController()->showAd(AdTypeBanner);
    }
}

// @005b9518
void Gameplay::addListeners()
{
    // @005bcf90 ($_0)
    _gameplayMenuActionListener = EventListenerCustom::create(
        "gameplayMenuAction", [this](EventCustom* event) {
            handleMenuAction((GameplayMenuAction) * (int*)event->getUserData());
        });
    _gameplayMenuActionListener->retain();
    getEventDispatcher()->addEventListenerWithSceneGraphPriority(_gameplayMenuActionListener, this);

    // @005bd020 ($_1)
    _gameplayControlsActionListener = EventListenerCustom::create(
        "gameplayControlsAction", [this](EventCustom* event) {
            handleControlsLayerAction((ControlsLayerAction) * (int*)event->getUserData());
        });
    _gameplayControlsActionListener->retain();
    getEventDispatcher()->addEventListenerWithSceneGraphPriority(_gameplayControlsActionListener,
                                                                 this);

    // @005bd0b0 ($_2)
    _levelCompletedListener = EventListenerCustom::create(
        "levelCompleted", [this](EventCustom* event) { handleLevelComplete(); });
    _levelCompletedListener->retain();
    getEventDispatcher()->addEventListenerWithSceneGraphPriority(_levelCompletedListener, this);

    // @005bd134 ($_3)
    _characterDyingListener = getEventDispatcher()->addCustomEventListener(
        "characterDying", [this](EventCustom* event) { handleDying(); });
    _characterDyingListener->retain();

    // @005bd1b8 ($_4)
    _characterDeadListener = getEventDispatcher()->addCustomEventListener(
        "characterDead", [this](EventCustom* event) { handleDead(); });
    _characterDeadListener->retain();

    // @005bd280 ($_5)
    _systemTriggerListener = getEventDispatcher()->addCustomEventListener(
        "system_trigger", [this](EventCustom* event) { systemTrigger(event->getUserData()); });
    _systemTriggerListener->retain();

    // @005bd30c ($_6)
    _characterEjectedListener = getEventDispatcher()->addCustomEventListener(
        "characterEjected", [this](EventCustom* event) { characterEjected(); });
    _characterEjectedListener->retain();
}

// @005b9a74
bool Gameplay::init(std::string levelXml)
{
    bool result = Layer::init();
    if (!result)
    {
        _debugLayer = nullptr;
        return result;
    }
    if (levelXml.empty())
    {
        // despite its name this holds the level XML text read by Settings::setSelectedLevel
        _levelXml = Settings::getInstance()->getSelectedLevelFilePath();
    }
    else
    {
        _levelXml = levelXml;
    }
    addChild(LayerColor::create(Color4B(globals::colors::blue, 255)), -100);
    return result;
}

// @005b9b68
void Gameplay::removeBanner(bool force)
{
    Settings* settings = Settings::getInstance();
    if (!settings->getShouldDisplayPersistentBannerDuringGameplay() || force)
    {
        settings->getAdController()->removeBannerAd();
    }
}

// @005b9bac
void Gameplay::die()
{
    removeListeners();
    unscheduleUpdate();
    Settings* settings = Settings::getInstance();
    settings->killSession();
    settings->getSoundController()->stopAllSounds(true);
}

// @005b9bec
void Gameplay::removeListeners()
{
    if (_gameplayMenuActionListener)
    {
        getEventDispatcher()->removeEventListener(_gameplayMenuActionListener);
        _gameplayMenuActionListener->release();
        _gameplayMenuActionListener = nullptr;
    }
    if (_gameplayControlsActionListener)
    {
        getEventDispatcher()->removeEventListener(_gameplayControlsActionListener);
        _gameplayControlsActionListener->release();
        _gameplayControlsActionListener = nullptr;
    }
    if (_levelCompletedListener)
    {
        getEventDispatcher()->removeEventListener(_levelCompletedListener);
        _levelCompletedListener->release();
        _levelCompletedListener = nullptr;
    }
    if (_characterDyingListener)
    {
        getEventDispatcher()->removeEventListener(_characterDyingListener);
        _characterDyingListener->release();
        _characterDyingListener = nullptr;
    }
    if (_characterDeadListener)
    {
        getEventDispatcher()->removeEventListener(_characterDeadListener);
        _characterDeadListener->release();
        _characterDeadListener = nullptr;
    }
    if (_systemTriggerListener)
    {
        getEventDispatcher()->removeEventListener(_systemTriggerListener);
        _systemTriggerListener->release();
        _systemTriggerListener = nullptr;
    }
    if (_characterEjectedListener)
    {
        getEventDispatcher()->removeEventListener(_characterEjectedListener);
        _characterEjectedListener->release();
        _characterEjectedListener = nullptr;
    }
}

// @005b9d3c
void Gameplay::checkCharacterPosition()
{
    if (_characterDroppedOffscreen)
    {
        return;
    }
    if (_session->getCamera()->getFocus()->GetPosition().y < -20.0f)
    {
        Director::getInstance()->getEventDispatcher()->dispatchCustomEvent("characterDead", nullptr);
        _characterDroppedOffscreen = true;
    }
}

// @005b9e20
void Gameplay::update(float dt)
{
    unsigned char state;
    if (_isReplay)
    {
        state = _replayData->getEntry();
        // ONLINE (PC addition): browser replays pick their byte per physics step
        // (online/replays/ReplayRuntime.h).
        if (online::flashLevel())
        {
            online::replays::gameplayState(_replayData, true, &state);
        }
        _controls->setState(state);
    }
    else
    {
        state = _controls->getState();
        // ONLINE (PC addition): browser levels also read Shift / Ctrl (user-vehicle actions) and
        // Z without an eject button (user-vehicle riders); see online/vehicles/UserVehicle.h.
        if (online::flashLevel())
        {
            state |= online::pcExtraControlBits();
            online::replays::gameplayState(_replayData, false, &state);  // records browser replays
        }
        _replayData->addEntry(state);
    }

    LevelB2D* level = _level;
    if (!level->getLevelComplete())
    {
        CharacterB2D* character = level->getCharacter();
        if (character && !character->getDead() && !_characterDroppedOffscreen && _timer->update())
        {
            checkCharacterPosition();
            std::vector<CharacterB2D*> characters = _level->getCharacters();
            for (unsigned int i = 0; i < characters.size(); i++)
            {
                characters[i]->setState(state);
            }
        }
    }

    _backgroundLayer->update(_session->getGameplayContainer()->getPosition());
    Settings::getInstance()->getCurrentSession()->update(dt);
}

// @005b9fb4
LevelB2D* Gameplay::getLevel()
{
    return _level;
}

// @005b9fbc
void Gameplay::pauseGameplay()
{
    if (_paused)
    {
        return;
    }
    // EDITOR (iOS port): -[GameplayLayer startPause] in test mode stops all sounds and pops back
    // to the level editor scene instead of showing the pause menu.
    if (_isTesting)
    {
        Settings::getInstance()->getSoundController()->stopAllSounds(false);
        removeBanner(true);
        die();
        Director::getInstance()->popScene();
        return;
    }
    _paused = true;
    Settings* settings = Settings::getInstance();
    settings->getAdController()->showAd(AdTypeBanner);
    _pauseLayer = PauseLayer::create();
    addChild(_pauseLayer, 12);
    settings->getSoundController()->setPaused(true);
    _controls->setHidden(true);
    _timer->setHidden(true);
    settings->getCurrentSession()->pauseEmitters();
    Node* highlightNode = getHighlightNode();
    if (highlightNode)
    {
        highlightNode->setVisible(false);
    }
    unscheduleUpdate();
}

// @005ba118
Node* Gameplay::getHighlightNode()
{
    if (!_highlightNode)
    {
        _highlightNode = Node::create();
        addChild(_highlightNode, 5);
    }
    return _highlightNode;
}

// @005ba160
void Gameplay::unpauseGameplay()
{
    if (!_paused)
    {
        return;
    }
    _paused = false;
    Settings* settings = Settings::getInstance();
    settings->getSoundController()->setPaused(false);
    Session* session = settings->getCurrentSession();
    session->resumeEmitters();
    if (!settings->getShouldDisplayPersistentBannerDuringGameplay() &&
        !session->getLevel()->getCharacter()->getDead() && !_characterDroppedOffscreen)
    {
        settings->getAdController()->removeBannerAd();
    }
    _controls->setHidden(false);
    _timer->setHidden(false);
    removeChild(_pauseLayer, true);
    _pauseLayer = nullptr;
    Node* highlightNode = getHighlightNode();
    if (highlightNode)
    {
        highlightNode->setVisible(true);
    }
    scheduleUpdate();
}

// @005ba270
void Gameplay::debugBtnPressed(Ref* sender)
{
    if (_debugLayer)
    {
        removeChild(_debugLayer, true);
        _debugLayer = nullptr;
    }
    else
    {
        _debugLayer = DebugLayer::create();
        addChild(_debugLayer, 11);
    }
}

// @005ba340
void Gameplay::handleMenuAction(GameplayMenuAction action)
{
    Scene* scene;
    switch (action)
    {
    case GameplayMenuActionResume:
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "options_pressed", "",
                                                            -1);
        unpauseGameplay();
        return;

    case GameplayMenuActionExit:
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "exit_level_pressed",
                                                            "", -1);
        removeBanner(true);
        die();
        // EDITOR (iOS port): leaving a user level (chapter 5000/5001) goes to the main menu.
        scene = MainMenu::createScene(LevelSession::getInstance()->isUserLevel() ? MenuModeMain
                                                                                : MenuModeLevelSelect,
                                      nullptr);
        break;

    case GameplayMenuActionReset:
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory,
                                                            "reset_level_pressed", "", -1);
        removeBanner(false);
        die();
        scene = ResetWorkaroundScene::createScene();
        break;

    case GameplayMenuActionChangeCharacter:
        removeBanner(true);
        die();
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory,
                                                            "change_character_pressed", "", -1);
        scene = CharacterSelectLayer::createScene(Settings::getInstance()->getSelectedCharacterId(), 0);
        break;

    case GameplayMenuActionViewReplay:
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory,
                                                            "view_replay_pressed", "", -1);
        _isReplay = true;
        _replayData->resetPosition();
        removeBanner(false);
        die();
        scene = Gameplay::createScene("", _replayData);
        break;

    case GameplayMenuActionNextLevel:
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory,
                                                            "advance_level_pressed", "", -1);
        // EDITOR (iOS port): advanceLevelIndex returns NO for user levels (no campaign successor).
        if (!LevelSession::getInstance()->isUserLevel() && Settings::getInstance()->advanceLevelIndex())
        {
            removeBanner(false);
            die();
            scene = Gameplay::createScene("", nullptr);
        }
        else
        {
            removeBanner(true);
            die();
            scene = MainMenu::createScene(MenuModeLevelSelect, nullptr);
        }
        break;

    default:
        return;
    }
    Director::getInstance()->replaceScene(scene);
}

// @005bab78
void Gameplay::exitToMenu()
{
    Director::getInstance()->replaceScene(MainMenu::createScene(MenuModeLevelSelect, nullptr));
}

// @005baba8
void Gameplay::exitToSelectCharacter()
{
    Director::getInstance()->replaceScene(
        CharacterSelectLayer::createScene(Settings::getInstance()->getSelectedCharacterId(), 0));
}

// @005babdc
void Gameplay::handleControlsLayerAction(ControlsLayerAction action)
{
    switch (action)
    {
    case ControlsLayerActionPause:
        pauseGameplay();
        break;
    case ControlsLayerActionReset:
        // EDITOR (iOS port): a test play restarts the editor's level (ResetWorkaroundScene would
        // load the selected campaign level).
        if (_isTesting)
        {
            removeBanner(false);
            die();
            Director::getInstance()->replaceScene(Gameplay::createTestingScene(_levelXml));
            break;
        }
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory,
                                                            "reset_level_pressed", "", -1);
        removeBanner(false);
        die();
        Director::getInstance()->replaceScene(ResetWorkaroundScene::createScene());
        break;
    }
}

// @005badbc
void Gameplay::handleDying()
{
    if (_dyingVignette)
    {
        _dyingVignette->removeFromParentAndCleanup(true);
    }
    _dyingVignette = DyingVignette::create();
    addChild(_dyingVignette, 3);
}

// @005bae78
void Gameplay::handleDead()
{
    _characterDead = true;
    if (_dyingVignette)
    {
        _dyingVignette->stop();
    }
    if (!_isTesting)
    {
        Settings::getInstance()->getAdController()->showAd(AdTypeBanner);
    }
    clearHighlights();
}

// @005baec4
void Gameplay::clearHighlights()
{
    Node* highlightNode = getHighlightNode();
    if (!highlightNode)
    {
        return;
    }
    Vector<Node*> highlights = highlightNode->getChildren();
    for (auto it = highlights.rbegin(); it != highlights.rend(); ++it)
    {
        static_cast<HighlightSprite*>(*it)->fadeOut();
    }
}

// @005bb028
void Gameplay::handleDebugLayerAction(DebugLayerAction action)
{
}

// @005bb02c
void Gameplay::handleLevelComplete()
{
    if (_levelComplete)
    {
        return;
    }
    _levelComplete = true;
    Settings* settings = Settings::getInstance();
    settings->getSoundController()->playSound("Victory", 1.0f, 1.0f, 0.0f);
    _controls->victory();
    _timer->setVisible(false);
    clearHighlights();
    if (_isTesting)
    {
        return;
    }
    // NET (PC addition): in a ghost race the race HUD shows the finish and the results.
    if (race::suppressVictoryMenu())
    {
        return;
    }

    float time = _timer->getTime();
    int placement;
    if (_isReplay)
    {
        placement = 0;
    }
    else
    {
        // EDITOR (iOS port): user levels have no LevelMO in the iOS Session, so no completion
        // time is recorded (addCompletionTime: is sent to nil).
        placement = LevelSession::getInstance()->isUserLevel() ? 0 : settings->addCompletionTime(time);
        int chapter = Settings::getInstance()->getSelectedChapter();
        int level = Settings::getInstance()->getSelectedLevel();
        std::string label = "level_" + patch::to_string(chapter) + "_" + patch::to_string(level);
        if (UserDefault::getInstance()->getBoolForKey("gore_disabled"))
        {
            Settings::getInstance()->getTracker()->submitAction(s_trackerCategory,
                                                                "victory_no_gore", label, (int)time);
        }
        else
        {
            Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "victory", label,
                                                                (int)time);
        }
    }

    _victoryMenu = new VictoryMenu();
    _victoryMenu->autorelease();
    _victoryMenu->init(time, placement, Size(Size::ZERO));
    addChild(_victoryMenu, 7);
    settings->getAdController()->showAd(AdTypeBanner);
}

// @005bb5cc
void Gameplay::draw(Renderer* renderer, const Mat4& transform, uint32_t flags)
{
    Node::draw(renderer, transform, flags);
    if (_session && _session->getDebugDraw())
    {
        GL::enableVertexAttribs(GL::VERTEX_ATTRIB_FLAG_POSITION);
        Director::getInstance()->pushMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
        Settings::getInstance()->getCurrentSession()->getWorld()->DrawDebugData();
        Director::getInstance()->popMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
    }
}

// @005bb634
void Gameplay::systemTrigger(void* data)
{
    if (!_controls)
    {
        return;
    }
    switch (*(int*)data)
    {
    case 0:
        highlightSpriteAtPos(_controls->getForwardPos(), 0);
        break;
    case 1:
        highlightSpriteAtPos(_controls->getBackwardPos(), 1);
        break;
    case 2:
        highlightSpriteAtPos(_controls->getLeanForwardPos(), 2);
        break;
    case 3:
        highlightSpriteAtPos(_controls->getLeanBackwardPos(), 3);
        break;
    case 4:
        highlightSpriteAtPos(_controls->getSpecialPos(), 4);
        break;
    case 5:
        clearHighlights();
        break;
    }
}

// @005bb6e4
void Gameplay::highlightSpriteAtPos(Vec2 position, int tag)
{
    if (_characterDead)
    {
        return;
    }
    Node* highlightNode = getHighlightNode();
    if (!highlightNode)
    {
        return;
    }
    HighlightSprite* highlight = HighlightSprite::create();
    float scale = Settings::getInstance()->getTabletControlsScale();
    highlight->setTag(tag);
    highlight->setScale(scale);
    highlight->setPosition(position);
    highlightNode->addChild(highlight);
}

// @005bb8bc
void Gameplay::characterEjected()
{
    clearHighlights();
    if (_characterEjectedListener)
    {
        getEventDispatcher()->removeEventListener(_characterEjectedListener);
        _characterEjectedListener->release();
        _characterEjectedListener = nullptr;
    }
}

// @005bb908
void Gameplay::onEnterTransitionDidFinish()
{
    Node::onEnterTransitionDidFinish();
    if (!_paused)
    {
        runAction(Sequence::create(
            DelayTime::create(1.0f / 30.0f),
            CallFunc::create(CC_CALLBACK_0(Gameplay::oneFrameAfterOnEnterTransitionDidFinish, this)),
            nullptr));
    }
}

// @005bba50
void Gameplay::oneFrameAfterOnEnterTransitionDidFinish()
{
    Settings::getInstance()->getAdController()->getAdsRemoved();  // result unused
    AdController* adController = Settings::getInstance()->getAdController();
    adController->setAdControllerDelegate(this);
    if (adController->showAd(AdTypeInterstitial))
    {
        removeBanner(true);
    }
    else
    {
        adController->setAdControllerDelegate(nullptr);
        beginGameplayFollowingInterstitial();
    }
}

// @005bbacc
void Gameplay::onExitTransitionDidStart()
{
    Node::onExitTransitionDidStart();
    _gameplayStarted = false;
    _exitTransitionStarted = true;
}

// @005bbaf4; thunk @005bc4dc
void Gameplay::interstitialDidEnd(InterstitialStatus status)
{
    AdController* adController = Settings::getInstance()->getAdController();
    adController->setAdControllerDelegate(nullptr);
    switch (status)
    {
    case InterstitialStatusEnded:
    case InterstitialStatusAdsRemoved:
    case InterstitialStatusExpired:
        adController->resetElapsedTime();
        break;
    case InterstitialStatusFailed:
        if (adController->getElapsedTimeHasExceededInterstitialInterval())
        {
            // no interstitial: show the house ad instead
            adController->resetElapsedTime();
            runAction(Sequence::create(
                DelayTime::create(0.25f),
                CallFunc::create(CC_CALLBACK_0(Gameplay::displayInternalAd, this)), nullptr));
            return;
        }
        break;
    default:
        break;
    }
    runAction(Sequence::create(
        DelayTime::create(0.25f),
        CallFunc::create(CC_CALLBACK_0(Gameplay::beginGameplayFollowingInterstitial, this)),
        nullptr));
}

// @005bbcb0
void Gameplay::displayInternalAd()
{
    if (!s_pleaseTurnOnDataShown)
    {
        s_pleaseTurnOnDataShown = true;
        _pleaseTurnOnDataWindow = Settings::getInstance()->createWindow(
            HWWindowAppearanceAlert, (HWWindowDelegate*)this, false, true);
        _pleaseTurnOnDataWindow->showAlertMessage(
            "Help us!?", OW_GAMETEXT(gameplayPleaseTurnOnData, 0x00412d1f), "I'll consider it.", "",
            true);
        return;
    }

    Texture2D::PixelFormat pixelFormat = Texture2D::getDefaultAlphaPixelFormat();
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);
    _internalAdNode = Node::create();
    Sprite* ad = Sprite::create("images/want_more_hw.png");
    ad->setTag(42);
    ad->setScale(1.5f);
    _internalAdNode->addChild(ad);
    Size visibleSize = Director::getInstance()->getVisibleSize();
    _internalAdNode->setPosition(Vec2(visibleSize.width * 0.5f, visibleSize.height * 1.5f));
    addChild(_internalAdNode, 10000000);
    Texture2D::setDefaultAlphaPixelFormat(pixelFormat);

    _internalAdCountdown = globals::advertising::internalAdDisplayTime;
    int seconds = globals::advertising::internalAdDisplayTime - 5;
    _internalAdLabel = Label::createWithTTF("continue in " + patch::to_string(seconds),
                                            "fonts/ClarendonLTStd-Bold.ttf", 85.0f, Size::ZERO,
                                            TextHAlignment::LEFT, TextVAlignment::TOP);
    _internalAdLabel->setAlignment(TextHAlignment::CENTER);
    _internalAdLabel->setColor(Color3B::WHITE);
    _internalAdLabel->setAnchorPoint(Vec2(0.5f, 1.0f));
    Size adSize = ad->getBoundingBox().size;
    Vec2 labelPosition(0.0f, adSize.height * -0.5f + -60.0f);
    _internalAdLabel->setPosition(labelPosition);
    _internalAdNode->addChild(_internalAdLabel);

    Label* info = Label::createWithTTF(OW_GAMETEXT(gameplayEnsureConnectivity, 0x00401800),
                                       "fonts/ClarendonLTStd.ttf", 70.0f, Size::ZERO,
                                       TextHAlignment::LEFT, TextVAlignment::TOP);
    info->setAlignment(TextHAlignment::CENTER);
    info->setColor(Color3B::WHITE);
    info->setAnchorPoint(Vec2(0.5f, 1.0f));
    info->setPosition(Vec2(0.0f, labelPosition.y + -100.0f));
    _internalAdNode->addChild(info);

    CallFunc* countDown = CallFunc::create(CC_CALLBACK_0(Gameplay::countDownInternalAd, this));
    FiniteTimeAction* tick = Sequence::create(DelayTime::create(1.0f), countDown, nullptr);
    CallFunc* timeout = CallFunc::create(CC_CALLBACK_0(Gameplay::internalAdError, this));
    _internalAdNode->runAction(
        Sequence::create(Repeat::create(tick, _internalAdCountdown), timeout, nullptr));
    _internalAdNode->runAction(EaseExponentialOut::create(
        MoveTo::create(0.5f, Vec2(visibleSize.width * 0.5f, visibleSize.height * 0.5f))));
    Settings::getInstance()->getAdController()->resetElapsedTime();
}

// @005bc4e4
void Gameplay::countDownInternalAd()
{
    _internalAdCountdown--;
    if (_internalAdCountdown < 6 && !_internalAdBtn)
    {
        addInternalAdBtn();
        _internalAdLabel->setString("press the 'x' button to continue");
    }
    else
    {
        int seconds = _internalAdCountdown - 5;
        if (seconds >= 0)
        {
            _internalAdLabel->setString("continue in " + patch::to_string(seconds));
        }
    }
}

// @005bc650
void Gameplay::internalAdError()
{
    internalAdComplete();
}

// @005bc654
void Gameplay::internalAdComplete()
{
    clearAndRemovePleaseTurnOnDataWindow();
    if (_continueBtnParent)
    {
        _continueBtnParent->removeFromParentAndCleanup(true);
        _continueBtnParent = nullptr;
    }
    if (_internalAdMenu)
    {
        _internalAdMenu->setEnabled(false);
    }
    if (_internalAdNode)
    {
        _internalAdNode->stopAllActions();
        removeChild(_internalAdNode, true);
        _internalAdNode = nullptr;
        _internalAdLabel = nullptr;
    }
    beginGameplayFollowingInterstitial();
}

// @005bc6f4
void Gameplay::clearAndRemovePleaseTurnOnDataWindow()
{
    if (_pleaseTurnOnDataWindow)
    {
        _pleaseTurnOnDataWindow->removeAllDelegates();
        _pleaseTurnOnDataWindow->removeFromParent();
        _pleaseTurnOnDataWindow = nullptr;
    }
}

// @005bc730
void Gameplay::showContinueButtonInCaseInterstitialDoesNotLoadAndNoFailureMessageIsReported()
{
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("menus/alert/menu_alert.plist");
    std::string closeFrame = "window_btn_close.png";
    MenuItemImage* continueBtn = MenuItemImage::create(
        "", "", CC_CALLBACK_1(Gameplay::internalAdContinueBtnPressed, this));
    Sprite* normalSprite = Sprite::createWithSpriteFrameName(closeFrame);
    Sprite* selectedSprite = Sprite::createWithSpriteFrameName(closeFrame);
    selectedSprite->setOpacity(0x7d);
    continueBtn->setNormalImage(normalSprite);
    continueBtn->setSelectedImage(selectedSprite);
    Menu* menu = Menu::create(continueBtn, nullptr);
    menu->setAnchorPoint(Vec2::ZERO);
    menu->setPosition(Vec2::ZERO);
    _continueBtnParent->addChild(menu);
    Size visibleSize = Director::getInstance()->getVisibleSize();
    continueBtn->setPosition(Vec2(visibleSize.width * 0.5f, visibleSize.height * 0.5f));
}

// @005bca34
void Gameplay::internalAdContinueBtnPressed(Ref* sender)
{
    internalAdComplete();
}

// @005bca38
void Gameplay::addInternalAdBtn()
{
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("menus/alert/menu_alert.plist");
    std::string closeFrame = "window_btn_close.png";
    _internalAdBtn = MenuItemImage::create(
        "", "", CC_CALLBACK_1(Gameplay::internalAdContinueBtnPressed, this));
    Sprite* normalSprite = Sprite::createWithSpriteFrameName(closeFrame);
    Sprite* selectedSprite = Sprite::createWithSpriteFrameName(closeFrame);
    selectedSprite->setOpacity(0x7d);
    _internalAdBtn->setNormalImage(normalSprite);
    _internalAdBtn->setSelectedImage(selectedSprite);
    _internalAdMenu = Menu::create(_internalAdBtn, nullptr);
    _internalAdMenu->setAnchorPoint(Vec2::ZERO);
    _internalAdMenu->setPosition(Vec2::ZERO);
    _internalAdNode->addChild(_internalAdMenu);

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();  // unused
    Rect btnBox = _internalAdBtn->getBoundingBox();
    Vec2 position(visibleSize.width * 0.5f, btnBox.size.height * 0.5f + 10.0f);
    Node* ad = _internalAdNode->getChildByTag(42);
    if (ad)
    {
        // top right corner of the ad
        position = Vec2(ad->getContentSize().width * 0.5f * ad->getScale(),
                        ad->getContentSize().height * 0.5f * ad->getScale());
    }
    _internalAdBtn->setScale(0.5f);
    _internalAdBtn->setPosition(position);
    _internalAdBtn->runAction(EaseElasticOut::create(ScaleTo::create(0.5f, 1.0f), 0.3f));
}

// @005bce48; thunk @005bcebc
void Gameplay::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    if (window && _gameplayStarted && !_exitTransitionStarted && _pleaseTurnOnDataWindow == window)
    {
        window->removeAllDelegates();
        clearAndRemovePleaseTurnOnDataWindow();
        internalAdComplete();
    }
}
