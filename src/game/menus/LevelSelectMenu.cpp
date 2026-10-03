#include "LevelSelectMenu.h"

#include "CharacterSelectLayer.h"
#include "CharacterSprite.h"
#include "CreditsLayer.h"
#include "GameText.h"
#include "Gameplay.h"
#include "Globals.h"
#include "HWWindow.h"
#include "LevelSelectBtn.h"
#include "MainMenu.h"
#include "MenuHelper.h"
#include "PageControl.h"
#include "Patch.h"
#include "PerspectiveCharacters.h"
#include "Settings.h"
#include "Sound.h"
#include "SoundController.h"
#include "Tracker.h"
#include "UserProgress.h"

#include "sdkbox/PluginGoogleAnalytics.h"

#include <algorithm>
#include <cctype>
#include <cmath>

USING_NS_CC;

// Tracker category of the level select (_INIT_12).
static std::string s_trackerCategory = "level_select";

// @005dfe10
LevelSelectMenu::LevelSelectMenu()
    : _levelUnavailableWindow(nullptr)
    , _levelLockedWindow(nullptr)
    , _mainMenu(nullptr)
    , _clippingNode(nullptr)
    , _namesHolder(nullptr)
    , _commentsHolder(nullptr)
    , _buyHolder(nullptr)
    , _comingSoonHolder(nullptr)
    , _touch(nullptr)
    , _startX(0.0f)
    , _holderStartX(0.0f)
    , _targetX(0.0f)
    , _menuIndex(0)
    , _menuSliding(false)
    , _prevIndex(0.0f)
    , _unlockedLevels()
    , _max(0)
    , _buttonsSBN(nullptr)
    , _chapterLevelBtns()
    , _buttonSBNPos(Vec2::ZERO)
    , _stampsSBNPos(Vec2::ZERO)
    , _pageControlPos(Vec2::ZERO)
    , _charactersLayerPos(Vec2::ZERO)
    , _chapters()
    , _charactersLayer(nullptr)
    , _levelBtns()
    , _pressedBtn(nullptr)
    , _pageControl(nullptr)
    , _unlockedFullGame(false)
    , _animatingChapterAdvance(false)
    , _showOffscreenElementsOnEnter(false)
    , _shakeDuration(0)
{
    _index = (float)Settings::getInstance()->getSelectedChapter();
}

// @005dff3c (D1), @005dffe8 (D0)
LevelSelectMenu::~LevelSelectMenu()
{
}

// @005e000c
LevelSelectMenu* LevelSelectMenu::create(PerspectiveCharacters* perspectiveCharacters, MainMenu* mainMenu)
{
    LevelSelectMenu* menu = new LevelSelectMenu();
    menu->init(perspectiveCharacters, mainMenu);
    menu->autorelease();
    return menu;
}

// @005e0074
bool LevelSelectMenu::init(PerspectiveCharacters* perspectiveCharacters, MainMenu* mainMenu)
{
    _mainMenu = mainMenu;
    _menuSliding = false;
    _animatingChapterAdvance = false;
    _charactersLayer = perspectiveCharacters;
    return true;
}

// @005e0090
void LevelSelectMenu::onEnter()
{
    Node::onEnter();

    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    std::string plist = "menus/level_select/menu_level_select.plist";
    if (!cache->isSpriteFramesWithFileLoaded(plist))
    {
        cache->addSpriteFramesWithFile(plist);
    }

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    // Everything scrolling sideways is clipped to the visible rect.
    Sprite* stencil = Sprite::create();
    stencil->setTextureRect(Rect(origin, visibleSize));
    stencil->setPosition(visibleSize / 2.0f);
    _clippingNode = ClippingNode::create(stencil);
    _clippingNode->setContentSize(getContentSize());
    addChild(_clippingNode);

    _buttonsSBN = Node::create();
    _clippingNode->addChild(_buttonsSBN);
    _comingSoonHolder = Node::create();
    _clippingNode->addChild(_comingSoonHolder);
    _commentsHolder = Node::create();
    _clippingNode->addChild(_commentsHolder);
    _namesHolder = Node::create();
    _clippingNode->addChild(_namesHolder);

    addMenu();

    _pageControl = PageControl::create("levSelect_dot.png", Vec2(visibleSize.width * 0.5f, 100.0f), 70.0f,
                                       (int)_chapters.size(), (int)_index);
    _pageControl->setDelegate(this);
    addChild(_pageControl);

    checkForChapterCompletion();
    MenuHelper::addBackBtn(this, 2, CC_CALLBACK_0(LevelSelectMenu::backBtnPressed, this));
    scheduleUpdate();
}

// @005e0410
void LevelSelectMenu::addMenu()
{
    _shakeDuration = 0;
    Size winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    (void)origin;
    Settings* settings = Settings::getInstance();
    _chapters = Settings::getInstance()->getAllChaptersData();
    _max = (unsigned int)_chapters.size();

    // Only used for its size.
    Sprite* icon = Sprite::createWithSpriteFrameName("levSelect_icon.png");

    if (_max == 0)
    {
        return;
    }

    float iconWidth = icon->getTextureRect().size.width;
    float halfIconWidth = iconWidth * 0.5f;
    Vec2 comingLaterPos = Vec2::ZERO;
    float topY = winSize.height - 50.0f - halfIconWidth;
    float chapterX = winSize.width + iconWidth * -5.0f - 80.0f - 50.0f;
    float buttonsX = halfIconWidth + chapterX;

    for (unsigned int chapter = 0; chapter < _max; chapter++)
    {
        ValueMap chapterData = _chapters[chapter].asValueMap();
        ValueVector levels = chapterData["levels"].asValueVector();
        bool hasLevels = false;

        // 15 slots: 5 columns x 3 rows. Empty slots get an outline button.
        float y = 0.0f;
        float lastBtnHeight = 0.0f;
        for (unsigned int level = 0; level < 15; level++)
        {
            LevelSelectBtn* btn;
            if (level < levels.size())
            {
                bool completed = settings->isLevelCompleted(chapter, level);
                bool unlocked = settings->isLevelUnlocked(chapter, level);
                btn = LevelSelectBtn::create(chapter, level, !unlocked, completed);
                _buttonsSBN->addChild(btn);
                _chapterLevelBtns[chapter].push_back(btn);
                ValueMap levelData = levels[level].asValueMap();  // copied, unused
                _levelBtns.push_back(btn);
                hasLevels = true;
            }
            else
            {
                btn = LevelSelectBtn::create(-1, -1, false, false);
                _buttonsSBN->addChild(btn);
                _levelBtns.push_back(btn);
            }

            float btnWidth = btn->getSize().width;
            float btnHeight = btn->getSize().height;
            lastBtnHeight = btn->getSize().height;
            unsigned int column = level % 5;
            unsigned int row = level / 5;
            y = topY - btnHeight * row + row * -20.0f;
            btn->setPosition(Vec2(buttonsX + btnWidth * column + column * 20, y));
            if (level == 7)
            {
                comingLaterPos = btn->getPosition();
            }
        }

        if (!hasLevels)
        {
            Label* comingLater = Label::createWithTTF("COMING LATER", "fonts/ClarendonLTStd-Bold.ttf", 75.0f);
            comingLater->setAlignment(TextHAlignment::CENTER);
            comingLater->setColor(Color3B::WHITE);
            comingLater->setAnchorPoint(Vec2(0.5f, 0.5f));
            comingLater->setPosition(Vec2(comingLaterPos.x, comingLaterPos.y - 3.0f));
            comingLater->setOpacity(204);
            _comingSoonHolder->addChild(comingLater);
        }

        float titleY = y - lastBtnHeight / 2.0f - 65.0f;
        if (titleY <= 0.0f)
        {
            titleY = winSize.height * 0.5f;
        }

        std::string name = chapterData["name"].asString();
        std::transform(name.begin(), name.end(), name.begin(), ::toupper);
        Label* title = Label::createWithTTF(name, "fonts/ClarendonLTStd-Bold.ttf", 120.0f);
        title->setAlignment(TextHAlignment::LEFT);
        title->setColor(Color3B(253, 129, 129));
        title->setAnchorPoint(Vec2(0.0f, 1.0f));
        title->setPosition(Vec2(chapterX + winSize.width * chapter * 1.5f, titleY));
        _namesHolder->addChild(title);

        std::string comment = chapterData["comment"].asString();
        Label* commentLabel = Label::createWithTTF(comment, "fonts/ClarendonLTStd.ttf", 75.0f);
        commentLabel->setDimensions(iconWidth * 5.0f + 80.0f, 600.0f);
        commentLabel->setAlignment(TextHAlignment::LEFT);
        commentLabel->setColor(Color3B::WHITE);
        commentLabel->setAnchorPoint(Vec2(0.0f, 1.0f));
        commentLabel->setPosition(Vec2(chapterX + winSize.width * chapter * 2.0f,
                                       title->getPosition().y - title->getContentSize().height));
        _commentsHolder->addChild(commentLabel);

        buttonsX += winSize.width;
    }
}

// @005e1538
void LevelSelectMenu::checkForChapterCompletion()
{
    Settings* settings = Settings::getInstance();
    if (settings->getLevelWasCompleted())
    {
        settings->setLevelWasCompleted(false);
        if (settings->getAllLevelsCompletedForChapter((int)_index))
        {
            _animatingChapterAdvance = true;
            startLevelsAnimation();
            return;
        }
    }

    if (settings->getAdvancedFromLastLevel())
    {
        _animatingChapterAdvance = true;
        runAction(Sequence::create(DelayTime::create(0.5f),
                                   CallFunc::create(CC_CALLBACK_0(LevelSelectMenu::advanceChapter, this)), nullptr));
    }
    _checkUnlockedLevelsOnEnterTransition = true;
}

// @005e16ec
void LevelSelectMenu::backBtnPressed()
{
    _mainMenu->levelSelectMenuExit();
}

// @005e16f4
void LevelSelectMenu::onEnterTransitionDidFinish()
{
    // (sic) Node::onEnterTransitionDidFinish() is not called.
    if (_checkUnlockedLevelsOnEnterTransition)
    {
        showAlertIfLevelsUnlocked();
        _checkUnlockedLevelsOnEnterTransition = false;
    }
}

// @005e1720
void LevelSelectMenu::showAlertIfLevelsUnlocked()
{
    Settings* settings = Settings::getInstance();
    std::vector<UnlockedLevelAnnouncement> announcements =
        settings->getUserProgress()->getUnlockedLevelAnnouncements();
    if (announcements.empty())
    {
        return;
    }

    size_t count = announcements.size();
    std::string message = OW_GAMETEXT(levelSelectLevelsUnlockedMessage, 0x40e4d9);
    std::string listIntro = count > 1 ? "s: " : ": ";
    message += listIntro;

    // "a", "a and b", "a, b, and c"
    for (size_t i = 0; i < announcements.size(); i++)
    {
        UnlockedLevelAnnouncement announcement = announcements[i];
        std::string levelName = announcement.levelName;
        size_t total = announcements.size();
        if (total != 1)
        {
            if (total == 2)
            {
                if (i == 1)
                {
                    message += " and ";
                }
            }
            else
            {
                if (i != 0)
                {
                    message += ", ";
                }
                if (i == total - 1)
                {
                    message += "and ";
                }
            }
        }
        message += levelName;
    }
    message += ".";

    std::string confirmLabel = "Go to level";
    if (count > 1)
    {
        confirmLabel += "s";
    }

    _levelsUnlockedWindow = settings->createWindow(HWWindowAppearanceAlert, nullptr, true, true);
    _levelsUnlockedWindow->addDelegate(this);
    _levelsUnlockedWindow->showAlertMessage("Hooray!", message, confirmLabel, "Oh boy", true);
}

// @005e1d78
void LevelSelectMenu::addTouchListeners()
{
    EventListenerTouchOneByOne* listener = EventListenerTouchOneByOne::create();
    listener->setSwallowTouches(true);
    listener->onTouchBegan = CC_CALLBACK_2(LevelSelectMenu::onTouchBegan, this);
    listener->onTouchMoved = CC_CALLBACK_2(LevelSelectMenu::onTouchMoved, this);
    listener->onTouchEnded = CC_CALLBACK_2(LevelSelectMenu::onTouchEnded, this);
    listener->onTouchCancelled = CC_CALLBACK_2(LevelSelectMenu::onTouchCancelled, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
}

// @005e1f5c
bool LevelSelectMenu::onTouchBegan(Touch* touch, Event* event)
{
    if (_animatingChapterAdvance)
    {
        return false;
    }

    Vec2 location = touch->getLocation();
    Vec2 buttonsLocation = _buttonsSBN->convertToNodeSpace(location);
    for (LevelSelectBtn* btn : _levelBtns)
    {
        if (btn->getTouch() == nullptr && btn->getHitArea().containsPoint(buttonsLocation))
        {
            btn->setTouch(touch);
            _pressedBtn = btn;
            break;
        }
    }

    if (_touch == nullptr)
    {
        _touch = touch;
        _prevIndex = _index;
        _startX = location.x;
    }
    return true;
}

// @005e2060
void LevelSelectMenu::onTouchMoved(Touch* touch, Event* event)
{
    Vec2 location = touch->getLocation();
    if (_touch == nullptr)
    {
        return;
    }

    Size winSize = Director::getInstance()->getWinSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    (void)origin;
    if (fabsf(location.x - _startX) > 30.0f)
    {
        float delta = (location.x - _startX) / winSize.width;
        float index = _prevIndex - delta;
        // Rubber band past the first / last chapter.
        if (index < 0.0f)
        {
            delta *= 0.2f;
        }
        else if (index > (float)(_max - 1))
        {
            delta *= 0.1f;
        }
        _index = _prevIndex - delta;

        if (_pressedBtn != nullptr)
        {
            _pressedBtn->setTouch(nullptr);
            _pressedBtn = nullptr;
        }
    }
}

// @005e2158
void LevelSelectMenu::onTouchEnded(Touch* touch, Event* event)
{
    Vec2 location = touch->getLocation();
    Size winSize = Director::getInstance()->getWinSize();

    if (_pressedBtn != nullptr)
    {
        levelBtnPressed(_pressedBtn->getChapter(), _pressedBtn->getLevel(), _pressedBtn);
        _pressedBtn->setTouch(nullptr);
        _pressedBtn = nullptr;
    }
    else if (fabsf(location.x - _startX) > 25.0f)
    {
        // Swipe.
        float delta = location.x - _startX;
        if (delta < 0.0f)
        {
            _index = fminf((float)(_max - 1), _prevIndex + 1.0f);
        }
        else if (delta > 0.0f)
        {
            _index = fmaxf(_prevIndex - 1.0f, 0.0f);
        }
    }
    else
    {
        // Tap: right half = next chapter, left half = previous one.
        if (location.x > winSize.width * 0.5f)
        {
            _index = fminf((float)(_max - 1), _prevIndex + 1.0f);
        }
        else if (location.x < winSize.width * 0.5f)
        {
            _index = fmaxf(_prevIndex - 1.0f, 0.0f);
        }
        else
        {
            _index = std::round(_index);
        }
    }
    _touch = nullptr;
}

// @005e2294
void LevelSelectMenu::onTouchCancelled(Touch* touch, Event* event)
{
    onTouchEnded(touch, event);
}

// @005e2298
void LevelSelectMenu::remoteTouchListeners()
{
    _eventDispatcher->removeEventListenersForTarget(this, false);
}

// @005e22a8
void LevelSelectMenu::startLevelsAnimation()
{
    // Pop the check mark of every level in the chapter, one after the other.
    std::vector<Node*> btns = _chapterLevelBtns[(int)_index];
    for (int i = 0; i < btns.size(); i++)
    {
        Node* check = btns[i]->getChildByTag(1);
        if (check == nullptr)
        {
            _animatingChapterAdvance = false;
            break;
        }

        DelayTime* delay = DelayTime::create(i * 0.1f);
        ActionInterval* scaleUp = EaseExponentialOut::create(ScaleTo::create(0.15f, 1.75f));
        ActionInterval* scaleDown = EaseExponentialIn::create(ScaleTo::create(0.15f, 1.0f));
        CallFunc* click = CallFunc::create(CC_CALLBACK_0(LevelSelectMenu::playClick, this));
        Sequence* sequence;
        if (i == 0)
        {
            CallFunc* complete = CallFunc::create(CC_CALLBACK_0(LevelSelectMenu::levelsAnimationComplete, this));
            if (btns.size() == 1)
            {
                sequence = Sequence::create(click, scaleUp, scaleDown, complete, nullptr);
            }
            else
            {
                sequence = Sequence::create(click, scaleUp, scaleDown, nullptr);
            }
        }
        else if (i == btns.size() - 1)
        {
            CallFunc* complete = CallFunc::create(CC_CALLBACK_0(LevelSelectMenu::levelsAnimationComplete, this));
            sequence = Sequence::create(delay, click, scaleUp, scaleDown, complete, nullptr);
        }
        else
        {
            sequence = Sequence::create(delay, click, scaleUp, scaleDown, nullptr);
        }
        check->runAction(sequence);
    }
}

// @005e27c0
void LevelSelectMenu::advanceChapter()
{
    _animatingChapterAdvance = false;
    Settings* settings = Settings::getInstance();
    // RE-TODO(@006123ec): Settings::advanceChapterIndex() returns bool in the binary (cset w0,hi);
    // M10's Settings.h declares it void - needs `bool` there for this to compile.
    if (settings->advanceChapterIndex())
    {
        _index = (float)settings->getSelectedChapter();
        settings->setAdvancedFromLastLevel(false);
        settings->getSoundController()->playSound("SwishUp");
    }
    else if (settings->getUserProgress()->getAllLevelsCompleted())
    {
        showCredits();
    }
}

// @005e296c
void LevelSelectMenu::showCredits()
{
    UserDefault* userDefault = UserDefault::getInstance();
    std::string key = "credits_seen";
    if (!userDefault->getBoolForKey(key.c_str()))
    {
        userDefault->setBoolForKey(key.c_str(), true);
        userDefault->flush();
        Director::getInstance()->replaceScene(
            TransitionFade::create(3.0f, CreditsLayer::createScene(true), Color3B(0, 0, 0)));
    }
}

// @005e2ab4
void LevelSelectMenu::playClick()
{
    Sound::playSound("LevelCompleteClick", 1.0f, 1.0f, 0.0f, false);
}

// @005e2b6c
void LevelSelectMenu::levelsAnimationComplete()
{
    stampAnimationBegin();
}

// @005e2b70
void LevelSelectMenu::stampAnimationBegin()
{
    Settings* settings = Settings::getInstance();
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("menus/level_select/stamp_of_success.plist");
    CharacterSprite* portrait = _charactersLayer->getForemostSprite();
    ValueMap characterData = settings->getSelectedCharacterData();
    float offsetX = characterData["red_x_offset_x"].asFloat();
    // (sic) the original reads "red_x_offset_x" for y as well.
    float offsetY = characterData["red_x_offset_x"].asFloat();
    if (offsetY == 0.0f && offsetX == 0.0f)
    {
        offsetY = 0.7f;
        offsetX = 0.65f;
    }

    Sprite* stamp = Sprite::createWithSpriteFrameName("stamp_x.png");
    stamp->setPosition(Vec2(offsetX * portrait->getTextureRect().size.width,
                            offsetY * portrait->getTextureRect().size.height));
    float rotation = -CCRANDOM_0_1() * 30.0f + 15.0f;
    stamp->setRotation(rotation - 30.0f);
    stamp->setScale(6.0f);

    ActionInterval* fadeIn = EaseExponentialIn::create(FadeTo::create(0.25f, 255));
    ActionInterval* slam = EaseExponentialIn::create(ScaleTo::create(0.25f, 1.0f));
    RotateTo* rotate = RotateTo::create(0.25f, rotation);
    Sequence* slamSequence =
        Sequence::create(slam, CallFunc::create(CC_CALLBACK_0(LevelSelectMenu::shake, this)), nullptr);
    stamp->runAction(rotate);
    stamp->runAction(fadeIn);
    stamp->runAction(slamSequence);

    Settings::getInstance()->getSoundController()->playSound("StampSlam3");
    portrait->addChild(stamp);
}

// @005e304c
void LevelSelectMenu::shake()
{
    _shakeDuration = 45;
}

// @005e3058
void LevelSelectMenu::stampAnimationComplete()
{
    Settings* settings = Settings::getInstance();
    if (settings->getUserProgress()->getAllLevelsCompleted())
    {
        _animatingChapterAdvance = false;
        showCredits();
    }
    else if (settings->getAdvancedFromLastLevel())
    {
        advanceChapter();
    }
    else
    {
        _animatingChapterAdvance = false;
    }
    showAlertIfLevelsUnlocked();
}

// @005e30b4
bool LevelSelectMenu::allLevelsCompleted()
{
    Settings* settings = Settings::getInstance();
    ValueMap chapterData = _chapters[(int)_index].asValueMap();
    ValueVector levels = chapterData["levels"].asValueVector();
    // (sic) the last two levels of the chapter are not checked.
    for (unsigned int i = 0; i < levels.size() - 2; i++)
    {
        if (!settings->isLevelCompleted((int)_index, i))
        {
            return false;
        }
    }
    return true;
}

// @005e32c0
LevelSelectBtn* LevelSelectMenu::createBtn(int chapter, int level, bool locked)
{
    return nullptr;
}

// @005e32c8
void LevelSelectMenu::slideInComplete()
{
    addTouchListeners();
}

// @005e32cc
void LevelSelectMenu::slideOutComplete()
{
    _eventDispatcher->removeEventListenersForTarget(this, false);
}

// @005e32dc
void LevelSelectMenu::setShowOffscreenElementsOnEnter(bool show)
{
    _showOffscreenElementsOnEnter = show;
}

// @005e32e4
void LevelSelectMenu::levelBtnPressed(int chapter, int level, LevelSelectBtn* btn)
{
    std::string label = "level_" + patch::to_string(chapter) + "_" + patch::to_string(level);
    sdkbox::PluginGoogleAnalytics::logEvent("level", "play", label, 0);

    Settings* settings = Settings::getInstance();
    if (level == -1)
    {
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "level_selected", "not_available", -1);
        HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, this, true, true);
        _levelUnavailableWindow = window;
        window->showAlertMessage("This level doesn't exist. Yet.",
                                 OW_GAMETEXT(levelSelectLevelDoesNotExistMessage, 0x4077ef), "Ok", "", true);
    }
    else if (btn->getLocked())
    {
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "level_selected", "locked", -1);
        UnlockLevelInstructions instructions = settings->getUserProgress()->getUnlockLevelInstructions(chapter, level);
        std::string title = "This level is locked.";
        if (instructions.text.length() == 0)
        {
            std::string message = "chapter " + patch::to_string(chapter) + " level " + patch::to_string(level) +
                                  " is locked. Complete more levels to unlock it.";
            HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, this, true, true);
            _levelLockedWindow = window;
            window->showAlertMessage(title, message, "Ok", "", true);
        }
        else
        {
            HWWindow* window = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, this, true, true);
            _levelLockedWindow = window;
            window->showAlertMessage(title, instructions.text, "Ok", "Go to levels", true);
            _lockedLevelChapter = instructions.chapterIndex;
        }
    }
    else
    {
        UserDefault::getInstance()->setIntegerForKey("selectedChapter", (int)_index);
        settings->setSelectedLevel(chapter, level);
        std::string trackLabel = "level_" + patch::to_string(chapter) + "_" + patch::to_string(level);
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "level_selected", trackLabel, -1);

        ValueMap chapterData = _chapters[(int)_index].asValueMap();
        int characterIndex = chapterData["characterIndex"].asInt();
        settings->getSoundController()->stopBackgroundMusic();
        Scene* scene;
        if (characterIndex == -1)
        {
            scene = CharacterSelectLayer::createScene(0, 0);
        }
        else
        {
            // Empty string: Gameplay uses the level XML that setSelectedLevel() loaded into Settings.
            scene = Gameplay::createScene("", nullptr);
        }
        Director::getInstance()->replaceScene(
            TransitionFade::create(globals::ui::menuFadeTime, scene, Color3B(0, 0, 0)));
    }
}

// @005e43a8
void LevelSelectMenu::update(float dt)
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    (void)origin;

    _charactersLayer->setIndex(_index);

    // Every layer eases 1/5 of the way towards its target each frame (parallax factors 1, 1.5, 2, 2.5).
    float buttonsTargetX = -_index * visibleSize.width;
    Vec2 buttonsPos(_buttonsSBN->getPosition().x + (buttonsTargetX - _buttonsSBN->getPosition().x) / 5.0f,
                    _buttonsSBN->getPosition().y);
    _buttonsSBN->setPosition(buttonsPos);
    _comingSoonHolder->setPosition(buttonsPos);
    if (fabsf(buttonsTargetX - _buttonsSBN->getPosition().x) < 10.0f)
    {
        _menuSliding = false;
    }
    else
    {
        _menuSliding = true;
    }

    float namesTargetX = visibleSize.width * _index * -1.5f;
    _namesHolder->setPosition(
        Vec2(_namesHolder->getPosition().x + (namesTargetX - _namesHolder->getPosition().x) / 5.0f,
             _namesHolder->getPosition().y));

    float commentsTargetX = -_index * visibleSize.width * 2.0f;
    _commentsHolder->setPosition(
        Vec2(_commentsHolder->getPosition().x + (commentsTargetX - _commentsHolder->getPosition().x) / 5.0f,
             _commentsHolder->getPosition().y));

    if (_buyHolder != nullptr)
    {
        float buyTargetX = visibleSize.width * _index * -2.5f;
        _buyHolder->setPosition(Vec2(_buyHolder->getPosition().x + (buyTargetX - _buyHolder->getPosition().x) / 5.0f,
                                     _buyHolder->getPosition().y));
    }

    _pageControl->setPage((int)_index);

    if (_shakeDuration == 0)
    {
        return;
    }
    if (_shakeDuration == 45)
    {
        _buttonSBNPos = _buttonsSBN->getPosition();
        _charactersLayerPos = _charactersLayer->getPosition();
    }
    _shakeDuration--;

    if (_shakeDuration == 0)
    {
        _charactersLayer->setPosition(Vec2::ZERO);
        _buttonsSBN->setPosition(_buttonSBNPos);
        std::vector<Node*> btns = _chapterLevelBtns[(int)_index];
        for (Node* btn : btns)
        {
            btn->setRotation(0.0f);
        }
        stampAnimationComplete();
    }
    else
    {
        float strength = (float)_shakeDuration / 45.0f;
        float amount = strength * -5.0f;
        _charactersLayer->setPosition(Vec2(_charactersLayerPos.x + CCRANDOM_0_1() * amount,
                                           _charactersLayerPos.y + CCRANDOM_0_1() * amount));
        std::vector<Node*> btns = _chapterLevelBtns[(int)_index];
        for (Node* btn : btns)
        {
            btn->setRotation(strength * (CCRANDOM_0_1() * 16.0f - 8.0f));
        }
    }
}

// @005e4b28 (thunk @005e4b68)
void LevelSelectMenu::hwWindowWasDismissed(HWWindow* window)
{
    if (window == _levelUnavailableWindow)
    {
        _levelUnavailableWindow = nullptr;
    }
    else if (window == _levelLockedWindow)
    {
        _levelLockedWindow = nullptr;
    }
    else if (window == _levelsUnlockedWindow)
    {
        _levelsUnlockedWindow = nullptr;
    }
}

// @005e4ba8 (thunk @005e4c08)
void LevelSelectMenu::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    if (window == _levelUnavailableWindow)
    {
        _levelUnavailableWindow = nullptr;
    }
    else if (window == _levelLockedWindow)
    {
        // Cancel button = "Go to levels": scroll to the chapter that unlocks this level.
        if (buttonTag == 0)
        {
            _index = (float)_lockedLevelChapter;
        }
        _levelLockedWindow = nullptr;
    }
    else if (buttonTag == 1 && window == _levelsUnlockedWindow)
    {
        // "Go to level(s)": the All Characters chapter.
        _index = 6.0f;
    }
}

// @005e4c68 (thunk @005e4c74)
void LevelSelectMenu::pageControl(PageControl* control, int page)
{
    _index = (float)page;
}
