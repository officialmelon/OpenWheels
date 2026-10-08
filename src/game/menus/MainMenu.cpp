#include "MainMenu.h"

// Brings the five resolution-tier Size statics into this TU (dynamically initialised by _INIT_13,
// as in the original).
#include "AppDelegate.h"
#include "GameText.h"
#include "Globals.h"
#include "HWWindow.h"
#include "InfoMenu.h"
#include "LevelSelectMenu.h"
#include "MenuHelper.h"
#include "OptionsMenu.h"
#include "PerspectiveCharacters.h"
#include "Settings.h"
#include "SoundController.h"
#include "Tracker.h"
// EDITOR (iOS port): level editor / user levels entry points (src/editor, iOS bundle art)
#include "EditorLayer.h"
#include "LevelSession.h"
#include "UserLevelSelectUIView.h"
#include "platform/common/EditorAssets.h"
// ONLINE (PC addition): online level browser (src/online)
#include "online/OnlineLevelBrowser.h"
#include "online/OnlineUi.h"
#include "net/race/RaceHooks.h"  // NET (PC addition): ghost race
#include "qol/CharacterChoice.h"  // QOL (PC addition)

USING_NS_CC;

// Tracker category of every main-menu action (_INIT_13).
static std::string s_trackerCategory = "main_menu";

// @005e767c
MainMenu::MainMenu()
    : _logo(nullptr)
    , _preloadProgressBar(nullptr)
    , _texturesToPreload()
    , _preloadIndex(0)
    , _perspectiveCharacters(nullptr)
    , _menu(nullptr)
    , _menuNode(nullptr)
    , _unk368(false)
    , _unk369(false)
    , _chapterMenu(nullptr)
{
}

// @005e76d0 (D1), @005e7770 (D0)
MainMenu::~MainMenu()
{
    _preloadIndex = 0;
    _chapterMenu = nullptr;
    _perspectiveCharacters = nullptr;
    _menuNode = nullptr;
    _preloadProgressBar = nullptr;
    _logo = nullptr;
    _menu = nullptr;
}

// @005e7794
Scene* MainMenu::createScene(MenuMode mode, Node* unused)
{
    // ONLINE (PC addition): leaving a level started from the online browser goes back to it.
    if (Scene* online = online::OnlineLevelBrowser::sceneForReturnFromLevel())
    {
        return online;
    }
    // EDITOR (PC addition): and a level started from Your Levels goes back there.
    if (Scene* userLevels = UserLevelSelectUIView::sceneForReturnFromLevel())
    {
        return userLevels;
    }
    Scene* scene = Scene::create();
    MainMenu* layer;
    switch (mode)
    {
    case MenuModeMain:
        layer = MainMenu::create(false);
        break;
    case MenuModeUnknown1:
        layer = MainMenu::create(false);
        break;
    case MenuModeLevelSelect:
        layer = MainMenu::create(true);
        layer->showLevelSelectMenu(true, false);
        break;
    default:
        return scene;
    }
    scene->addChild(layer);
    return scene;
}

// @005e78f0
MainMenu* MainMenu::create(bool showLevelSelectMenu)
{
    MainMenu* layer = new (std::nothrow) MainMenu();
    if (layer != nullptr)
    {
        if (layer->init(showLevelSelectMenu))
        {
            layer->autorelease();
        }
        else
        {
            delete layer;
            layer = nullptr;
        }
    }
    return layer;
}

// @005e7984
void MainMenu::showLevelSelectMenu(bool show, bool animated)
{
    if (_chapterMenu == nullptr && show)
    {
        _chapterMenu = LevelSelectMenu::create(_perspectiveCharacters, this);
        _chapterMenu->stopAllActions();
        addChild(_chapterMenu, 3);
    }
    _chapterMenu->stopAllActions();
    _menuNode->stopAllActions();

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    (void)origin;
    Vec2 offscreenRight(visibleSize.width, 0.0f);
    Vec2 offscreenLeft(-visibleSize.width, 0.0f);

    if (show)
    {
        _perspectiveCharacters->setIsMainMenu(false);
        if (!animated)
        {
            _menuNode->setPosition(offscreenLeft);
            _chapterMenu->setPosition(Vec2::ZERO);
            _chapterMenu->slideInComplete();
            return;
        }
        _chapterMenu->setPosition(offscreenRight);
        _menuNode->setPosition(Vec2::ZERO);
        _menuNode->runAction(EaseExponentialInOut::create(MoveTo::create(0.5f, offscreenLeft)));
        _chapterMenu->runAction(Sequence::create(EaseExponentialInOut::create(MoveTo::create(0.5f, Vec2::ZERO)),
                                                 // @005ea500 ($_0)
                                                 CallFunc::create([this]() { _chapterMenu->slideInComplete(); }),
                                                 nullptr));
    }
    else
    {
        _perspectiveCharacters->setIsMainMenu(true);
        if (!animated)
        {
            _chapterMenu->setPosition(offscreenRight);
            _menuNode->setPosition(Vec2::ZERO);
            _chapterMenu->slideOutComplete();
            return;
        }
        _chapterMenu->setPosition(Vec2::ZERO);
        _menuNode->setPosition(offscreenLeft);
        _menuNode->runAction(EaseExponentialInOut::create(MoveTo::create(0.5f, Vec2::ZERO)));
        _chapterMenu->runAction(Sequence::create(EaseExponentialInOut::create(MoveTo::create(0.5f, offscreenRight)),
                                                 // @005ea588 ($_1)
                                                 CallFunc::create([this]() { _chapterMenu->slideOutComplete(); }),
                                                 nullptr));
    }
}

// @005e7c90
bool MainMenu::init(bool showLevelSelectMenu)
{
    if (!Layer::init())
    {
        return false;
    }

    _showBetaMessage = false;
    _menuNode = Node::create();
    addChild(_menuNode, 2);
    _chapterMenu = nullptr;
    _perspectiveCharacters = nullptr;

    // EDITOR (iOS port): back on the main menu no user level stays selected (iOS MainMenuLayer
    // resets chapterIndex 5000/5001 to 0 in addPerspectiveCharacters).
    if (LevelSession::getInstance()->isUserLevel())
    {
        LevelSession::getInstance()->setChapterIndex(0);
        qol::restorePlayerCharacter();  // QOL (PC addition): the player's own character again
    }
    LevelSession::getInstance()->clearLevelData();

    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    std::string plist = "menus/main/menu_main.plist";
    if (!cache->isSpriteFramesWithFileLoaded(plist))
    {
        cache->addSpriteFramesWithFile(plist);
    }

    static bool s_introMusicStarted = false;
    if (!s_introMusicStarted)
    {
        if (!UserDefault::getInstance()->getBoolForKey("intro_music_disabled"))
        {
            Settings::getInstance()->getSoundController()->playBackgroundMusic("");
        }
        s_introMusicStarted = true;
    }

    MenuHelper::addBg(this, 0);
    addPerspectiveCharacters();
    addMenu(!showLevelSelectMenu);
    if (showLevelSelectMenu)
    {
        this->showLevelSelectMenu(true, false);
    }
    return true;
}

// @005e7e60
void MainMenu::addPerspectiveCharacters()
{
    if (_perspectiveCharacters != nullptr)
    {
        _perspectiveCharacters->removeFromParent();
    }

    std::vector<int> characterIds;
    ValueVector chapters = Settings::getInstance()->getAllChaptersData();
    for (size_t i = 0; i < chapters.size(); i++)
    {
        ValueMap chapter = chapters[i].asValueMap();
        characterIds.push_back(chapter["characterIndex"].asInt());
    }

    // RESTORED (PC addition): the row is indexed by position; OpenWheels' campaign chapters
    // (src/restored) have chapter indices 100+.
    int selectedPosition = Settings::getInstance()->getChapterPosition(Settings::getInstance()->getSelectedChapter());
    _perspectiveCharacters = PerspectiveCharacters::create(characterIds, std::max(0, selectedPosition), true);
    _perspectiveCharacters->setPosition(0.0f, 0.0f);
    addChild(_perspectiveCharacters);
}

// @005e83a4
void MainMenu::addMenu(bool animated)
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    _logo = Sprite::createWithSpriteFrameName("menu_main_logo.png");
    _logo->setPosition(origin.x + visibleSize.width - _logo->getTextureRect().size.width * 0.5f - 70.0f,
                       origin.y + visibleSize.height - _logo->getTextureRect().size.height * 0.5f - 70.0f);
    if (animated)
    {
        Sequence* scaleIn = Sequence::create(EaseBounceOut::create(ScaleTo::create(1.0f, 1.0f)),
                                             CallFunc::create(CC_CALLBACK_0(MainMenu::logoScaleInComplete, this)),
                                             nullptr);
        _logo->setScale(0.01f);
        _logo->runAction(scaleIn);
    }
    _menuNode->addChild(_logo);

    MenuItemSprite* playBtn = btnWithIcon("menu_main_icon_play.png", ColorBlue, true, 0);
    MenuItemSprite* optionsBtn = btnWithIcon("menu_main_icon_options.png", ColorGrey, false, 1);
    MenuItemSprite* infoBtn = btnWithIcon("menu_main_icon_info.png", ColorPink, false, 2);
    playBtn->setAnchorPoint(Vec2(1.0f, 0.0f));
    optionsBtn->setAnchorPoint(Vec2(1.0f, 0.0f));
    infoBtn->setAnchorPoint(Vec2(1.0f, 0.0f));

    // (sic) two terminators: the original passes an extra null argument.
    _menu = Menu::create(infoBtn, optionsBtn, playBtn, nullptr, nullptr);
    _menu->setPosition(origin + Vec2::ZERO);

    // Right-aligned row at y = 70, 70 px margins.
    Vec2 playPos(origin.x + visibleSize.width - 70.0f, 70.0f);
    Vec2 optionsPos(playPos.x - playBtn->getContentSize().width - 70.0f, 70.0f);
    Vec2 infoPos(optionsPos.x - optionsBtn->getContentSize().width - 70.0f, 70.0f);
    // The original also queries the info button's size (presumably for a next position); unused.
    infoBtn->getContentSize();
    playBtn->setPosition(playPos);
    optionsBtn->setPosition(optionsPos);
    infoBtn->setPosition(infoPos);
    _menuNode->addChild(_menu, 1);

    // ONLINE (PC addition): left end of the row so far (the online button goes next to it).
    float rowLeft = infoPos.x - infoBtn->getContentSize().width;

    // EDITOR (iOS port): iOS MainMenuLayer has an editor button (tag 4 -> [EditorLayer scene]);
    // here a pink button (the iOS mainMenu_editorBtn colour) with the Android atlas'
    // menu_main_icon_editor.png, tag 3 (MainMenu::editorBtnPressed), plus the user-level list
    // (grey, play icon, tag 4). Only when the iOS editor art can be loaded.
    if (EditorAssets::loadAtlas("editorui") && EditorAssets::loadAtlas("levelEditorObjects1"))
    {
        MenuItemSprite* editorBtn = btnWithIcon("menu_main_icon_editor.png", ColorPink, false, 3);
        MenuItemSprite* userLevelsBtn = btnWithIcon("menu_main_icon_play.png", ColorGrey, false, 4);
        // The play icon frame is laid out for the wide play button (its untrimmed size is that
        // button's), so centre just its trimmed art on the square button.
        if (SpriteFrame* playFrame = SpriteFrameCache::getInstance()->getSpriteFrameByName("menu_main_icon_play.png"))
        {
            for (Node* child : Vector<Node*>(userLevelsBtn->getChildren()))
                if (child != userLevelsBtn->getNormalImage() && child != userLevelsBtn->getSelectedImage())
                    userLevelsBtn->removeChild(child, true);
            Sprite* icon = Sprite::createWithTexture(playFrame->getTexture(), playFrame->getRect(), playFrame->isRotated());
            icon->setPosition(userLevelsBtn->getContentSize() / 2.0f);
            userLevelsBtn->addChild(icon);
        }
        editorBtn->setAnchorPoint(Vec2(1.0f, 0.0f));
        userLevelsBtn->setAnchorPoint(Vec2(1.0f, 0.0f));
        Vec2 editorPos(infoPos.x - infoBtn->getContentSize().width - 70.0f, 70.0f);
        Vec2 userLevelsPos(editorPos.x - editorBtn->getContentSize().width - 70.0f, 70.0f);
        editorBtn->setPosition(editorPos);
        userLevelsBtn->setPosition(userLevelsPos);
        rowLeft = userLevelsPos.x - userLevelsBtn->getContentSize().width;  // ONLINE (PC addition)
        _menu->addChild(editorBtn);
        _menu->addChild(userLevelsBtn);
        if (animated)
        {
            editorBtn->setPosition(editorPos.x + visibleSize.width, editorPos.y);
            userLevelsBtn->setPosition(userLevelsPos.x + visibleSize.width, userLevelsPos.y);
            editorBtn->runAction(EaseExponentialOut::create(MoveTo::create(0.35f, editorPos)));
            userLevelsBtn->runAction(EaseExponentialOut::create(MoveTo::create(0.35f, userLevelsPos)));
        }
    }

    // ONLINE (PC addition): online levels (blue, generated globe icon, tag 5), always shown.
    {
        MenuItemSprite* onlineBtn = btnWithIcon("menu_main_icon_options.png", ColorBlue, false, 5);
        online::ui::setMenuButtonIcon(onlineBtn, "globe");
        onlineBtn->setAnchorPoint(Vec2(1.0f, 0.0f));
        Vec2 onlinePos(rowLeft - 70.0f, 70.0f);
        onlineBtn->setPosition(onlinePos);
        _menu->addChild(onlineBtn);
        if (animated)
        {
            onlineBtn->setPosition(onlinePos.x + visibleSize.width, onlinePos.y);
            onlineBtn->runAction(EaseExponentialOut::create(MoveTo::create(0.35f, onlinePos)));
        }
        rowLeft = onlinePos.x - onlineBtn->getContentSize().width;  // NET (PC addition)
    }

    // NET (PC addition): ghost race with nearby players (pink, checkered-flag icon, tag 6).
    {
        MenuItemSprite* raceBtn = btnWithIcon("menu_main_icon_options.png", ColorPink, false, 6);
        online::ui::setMenuButtonIcon(raceBtn, "flag");
        raceBtn->setAnchorPoint(Vec2(1.0f, 0.0f));
        Vec2 racePos(rowLeft - 70.0f, 70.0f);
        raceBtn->setPosition(racePos);
        _menu->addChild(raceBtn);
        if (animated)
        {
            raceBtn->setPosition(racePos.x + visibleSize.width, racePos.y);
            raceBtn->runAction(EaseExponentialOut::create(MoveTo::create(0.35f, racePos)));
        }
    }

    if (animated)
    {
        playBtn->setPosition(playPos.x + visibleSize.width, playPos.y);
        optionsBtn->setPosition(optionsPos.x + visibleSize.width, optionsPos.y);
        infoBtn->setPosition(infoPos.x + visibleSize.width, infoPos.y);
        infoBtn->runAction(Sequence::create(DelayTime::create(0.0f),
                                            EaseExponentialOut::create(MoveTo::create(0.35f, infoPos)), nullptr));
        optionsBtn->runAction(Sequence::create(DelayTime::create(0.1f),
                                               EaseExponentialOut::create(MoveTo::create(0.35f, optionsPos)),
                                               nullptr));
        playBtn->runAction(Sequence::create(DelayTime::create(0.2f),
                                            EaseExponentialOut::create(MoveTo::create(0.35f, playPos)), nullptr));
    }
}

// @005e89e8
void MainMenu::update(float dt)
{
}

// @005e89ec
void MainMenu::levelSelectMenuExit()
{
    showLevelSelectMenu(false, true);
}

// @005e89f8
void MainMenu::showLevelSelectMenuInComplete()
{
    _chapterMenu->slideInComplete();
}

// @005e8a00
void MainMenu::showLevelSelectMenuOutComplete()
{
    _chapterMenu->slideOutComplete();
}

// @005e8a08
void MainMenu::showMenu(bool show)
{
    _menu->setEnabled(show);
    _menu->setVisible(show);
    _logo->setOpacity(show ? 255 : 63);
}

// @005e8a64
void MainMenu::logoScaleInComplete()
{
    static bool s_betaMessageShown = false;

    std::string betaMessageKey = "beta_message_4";
    std::string startMessageKey = "start_message_1";
    UserDefault* userDefault = UserDefault::getInstance();
    bool betaMessageSeen = userDefault->getBoolForKey(betaMessageKey.c_str());
    if (!userDefault->getBoolForKey(startMessageKey.c_str()))
    {
        HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("New levels!", "Two new Pogo Stick Guy levels, 8 and 9, have been added.", "Ok",
                                 "", true);
        userDefault->setBoolForKey(startMessageKey.c_str(), true);
        userDefault->flush();
    }
    else if (_showBetaMessage && !(betaMessageSeen || s_betaMessageShown))
    {
        s_betaMessageShown = true;
        HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
        window->showAlertMessage("Oh hi, Mark.", OW_GAMETEXT(mainMenuBetaPogoLevelsRemovedMessage, 0x40640d),
                                 "Got it", "", true);
        userDefault->setBoolForKey(betaMessageKey.c_str(), true);
        userDefault->flush();
    }
}

// @005e8f74
MenuItemSprite* MainMenu::btnWithIcon(std::string iconFrameName, Color color, bool isPlayBtn, int tag)
{
    std::string normalFrame;
    std::string selectedFrame;
    if (isPlayBtn)
    {
        normalFrame = "menu_main_playbtn_blue_normal.png";
        selectedFrame = "menu_main_playbtn_blue_down.png";
    }
    else if (color == ColorBlue)
    {
        normalFrame = "menu_main_btn_blue_normal.png";
        selectedFrame = "menu_main_btn_blue_down.png";
    }
    else if (color == ColorPink)
    {
        normalFrame = "menu_main_btn_pink_normal.png";
        selectedFrame = "menu_main_btn_pink_down.png";
    }
    else if (color == ColorGrey)
    {
        normalFrame = "menu_main_btn_grey_normal.png";
        selectedFrame = "menu_main_btn_grey_down.png";
    }

    MenuItemImage* btn = MenuItemImage::create("", "", CC_CALLBACK_1(MainMenu::btnPressed, this));
    Sprite* normal = Sprite::createWithSpriteFrameName(normalFrame);
    Sprite* selected = Sprite::createWithSpriteFrameName(selectedFrame);
    Sprite* icon = Sprite::createWithSpriteFrameName(iconFrameName);
    icon->setAnchorPoint(Vec2(0.0f, 0.0f));
    btn->setNormalImage(normal);
    btn->setSelectedImage(selected);
    btn->setTag(tag);
    btn->addChild(icon);
    return btn;
}

// @005e9264
void MainMenu::preloadTextures()
{
    _texturesToPreload.push_back("test/preloading_textures/pattern1_1024x0124.png");
    _texturesToPreload.push_back("test/preloading_textures/pattern2_1024x0124.png");
    _texturesToPreload.push_back("test/preloading_textures/pattern3_1024x0124.png");
    _texturesToPreload.push_back("test/preloading_textures/pattern4_1024x0124.png");
    _texturesToPreload.push_back("test/preloading_textures/pattern5_1024x0124.png");
    _texturesToPreload.push_back("test/preloading_textures/pattern6_1024x0124.png");
    _texturesToPreload.push_back("test/preloading_textures/pattern7_1024x0124.png");

    // Result unused (leftover of the preloading test).
    FileUtils::getInstance()->getFileSize("test/preloading_textures/pattern1_1024x0124.png");
    FileUtils::getInstance()->getFileSize("test/preloading_textures/pattern2_1024x0124.png");
    FileUtils::getInstance()->getFileSize("test/preloading_textures/pattern3_1024x0124.png");
    FileUtils::getInstance()->getFileSize("test/preloading_textures/pattern4_1024x0124.png");
    FileUtils::getInstance()->getFileSize("test/preloading_textures/pattern5_1024x0124.png");
    FileUtils::getInstance()->getFileSize("test/preloading_textures/pattern6_1024x0124.png");
    FileUtils::getInstance()->getFileSize("test/preloading_textures/pattern7_1024x0124.png");

    preloadNextTexture();
}

// @005e9908
void MainMenu::preloadNextTexture()
{
    std::string path = _texturesToPreload.at(_preloadIndex);
    if (_preloadIndex != 0)
    {
        _preloadProgressBar->stopAllActions();
        _preloadProgressBar->runAction(
            ProgressTo::create(0.1f, ((float)_preloadIndex / (float)_texturesToPreload.size()) * 100.0f));
    }

    if (_preloadIndex == _texturesToPreload.size() - 1)
    {
        Director::getInstance()->getTextureCache()->addImageAsync(
            path, CC_CALLBACK_0(MainMenu::preloadTexturesComplete, this));
    }
    else
    {
        Director::getInstance()->getTextureCache()->addImageAsync(
            path, CC_CALLBACK_0(MainMenu::preloadNextTexture, this));
    }
    _preloadIndex++;
}

// @005e9b8c
void MainMenu::preloadTexturesComplete()
{
    _preloadIndex = 0;
    _preloadProgressBar->stopAllActions();
    _preloadProgressBar->runAction(ProgressTo::create(0.1f, 100.0f));
}

// @005e9be0
void MainMenu::btnPressed(Ref* sender)
{
    switch (static_cast<Node*>(sender)->getTag())
    {
    case 0:
        playBtnPressed();
        break;
    case 1:
        optionsBtnPressed();
        break;
    case 2:
        infoBtnPressed();
        break;
    case 3:
        editorBtnPressed();
        break;
    case 4:  // EDITOR (iOS port): user levels
        Director::getInstance()->replaceScene(UserLevelSelectUIView::scene());
        break;
    case 5:  // ONLINE (PC addition): online level browser
        Director::getInstance()->replaceScene(TransitionFade::create(
            globals::ui::menuFadeTime, online::OnlineLevelBrowser::createScene(), Color3B(0, 0, 0)));
        break;
    case 6:  // NET (PC addition): ghost race
        race::openRaceMenu();
        break;
    default:
        break;
    }
}

// @005e9c64
void MainMenu::playBtnPressed()
{
    Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "play_pressed", "", -1);
    showLevelSelectMenu(true, true);
}

// @005e9dc4
void MainMenu::infoBtnPressed()
{
    Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "info_pressed", "", -1);
    Director::getInstance()->replaceScene(
        TransitionFade::create(globals::ui::menuFadeTime, InfoMenu::createScene(), Color3B(0, 0, 0)));
}

// @005e9f64
void MainMenu::optionsBtnPressed()
{
    Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "options_pressed", "", -1);
    Director::getInstance()->pushScene(
        TransitionFade::create(globals::ui::menuFadeTime, OptionsMenu::createScene(), Color3B(0, 0, 0)));
}

// @005ea100
void MainMenu::editorBtnPressed()
{
    Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "editor_pressed", "", -1);
    // EDITOR (iOS port): -[MainMenuLayer handleBtnPress:] tag 4 -> replaceScene:[EditorLayer scene].
    Director::getInstance()->replaceScene(EditorLayer::createScene());
}

// @005ea240
void MainMenu::draw(Renderer renderer, const Mat4& transform, bool transformUpdated)
{
    GL::enableVertexAttribs(GL::VERTEX_ATTRIB_FLAG_POSITION);
    Director* director = Director::getInstance();
    director->pushMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
    director->popMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
}

// @005ea278
Sprite* MainMenu::addBgToNode(Node* node, int zOrder)
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    (void)origin;

    Texture2D::PixelFormat previousFormat = Texture2D::getDefaultAlphaPixelFormat();
    Texture2D::setDefaultAlphaPixelFormat(Texture2D::PixelFormat::RGBA8888);

    Sprite* bg = Sprite::create("menus/menu_main_bg.png");
    bg->setAnchorPoint(Vec2(0.0f, 0.0f));
    bg->setScale(visibleSize.width / bg->getTextureRect().size.width,
                 visibleSize.height / bg->getTextureRect().size.height);
    node->addChild(bg, zOrder);

    Texture2D::setDefaultAlphaPixelFormat(previousFormat);
    return bg;
}

// @005ea3c0
void MainMenu::addDebugUI()
{
}

// @005ea3c4
void MainMenu::sliderEvent(Ref* sender, ui::Slider::EventType type)
{
    if (type == ui::Slider::EventType::ON_PERCENTAGE_CHANGED)
    {
        ui::Slider* slider = dynamic_cast<ui::Slider*>(sender);
        int percent = slider->getPercent();
        float value = (float)percent / 100.0f;
        switch (slider->getTag())
        {
        case 0:
            _perspectiveCharacters->setZC(value);
            break;
        case 1:
            _perspectiveCharacters->setOffsetX(value);
            break;
        case 2:
            _perspectiveCharacters->setEyeX(value);
            break;
        case 3:
            _perspectiveCharacters->setSpaceZ(value);
            break;
        }
    }
}
