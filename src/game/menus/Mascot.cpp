#include "Mascot.h"

#include "AdController.h"
#include "GameText.h"
#include "HWWindow.h"
#include "Patch.h"
#include "Settings.h"
#include "SoundController.h"
#include "Tracker.h"

#include "audio/include/AudioEngine.h"

#include <cmath>

USING_NS_CC;

// File statics (_INIT_14).
static std::string s_mascotStateKey = "mascot_state";
static std::string s_mascotReviveCountKey = "mascot_revive_count";
static std::string s_trackerCategory = "popup_window";
static std::string s_necromancerKey = "necromancer";

// @005ea924
Mascot::Mascot()
    : _numGrunts(16)
    , _gruntAudioId(-1)
    , _lastGruntIndex(-1)
    , _gruntDurationKnown(false)
    , _hitCount(0)
    , _hitsToKill(4)
    , _unk320(false)
    , _mascotState(MascotStateDefault)
    , _unk328(0.0f)
    , _unk32c(100.0f)
    , _defaultGruntDuration(2.0f)
    , _body(nullptr)
    , _face(nullptr)
    , _armLeft(nullptr)
    , _armRight(nullptr)
    , _legLeft(nullptr)
    , _legRight(nullptr)
    , _flair(nullptr)
    , _wavePhase(0.0f)
    , _touchListener(nullptr)
    , _reviveWindow(nullptr)
    , _loadingAdWindow(nullptr)
    , _adFailedWindow(nullptr)
{
}

// @005ea9bc (D1), @005eaa64 (D0)
Mascot::~Mascot()
{
    removeListeners();
}

// @005eaa24
void Mascot::removeListeners()
{
    if (_touchListener != nullptr)
    {
        Director::getInstance()->getEventDispatcher()->removeEventListener(_touchListener);
        _touchListener->release();
        _touchListener = nullptr;
    }
}

// @005eaa88
bool Mascot::init()
{
    SpriteFrameCache::getInstance()->addSpriteFramesWithFile("menus/alert/mascot/mascot.plist");

    _body = Sprite::createWithSpriteFrameName("mascot_body.png");
    Size size = _body->getContentSize();
    _face = Sprite::createWithSpriteFrameName("mascot_default.png");

    _armLeft = Sprite::createWithSpriteFrameName("mascot_arm_left.png");
    _armLeft->setPosition(Vec2(size.width * -0.25f, size.height * 0.05f));
    _armRight = Sprite::createWithSpriteFrameName("mascot_arm_right.png");
    _armRight->setPosition(Vec2(size.width * 0.25f, 0.0f));
    _legLeft = Sprite::createWithSpriteFrameName("mascot_leg_left.png");
    _legLeft->setPosition(Vec2(size.width * -0.35f, size.height * -0.25f));
    _legRight = Sprite::createWithSpriteFrameName("mascot_leg_right.png");
    _legRight->setPosition(Vec2(size.width * 0.15f, size.height * -0.25f));

    addChild(_legRight);
    addChild(_armRight);
    addChild(_body);
    addChild(_face);
    addChild(_armLeft);
    addChild(_legLeft);

    addListeners();
    scheduleUpdate();

    UserDefault* userDefault = UserDefault::getInstance();
    setMascotState((MascotState)userDefault->getIntegerForKey(s_mascotStateKey.c_str()));
    if (userDefault->getBoolForKey(s_necromancerKey.c_str()))
    {
        addFlair(MascotFlairTinyHat);
    }
    return true;
}

// @005eaea8
void Mascot::addListeners()
{
    if (_touchListener != nullptr)
    {
        return;
    }

    _touchListener = EventListenerTouchOneByOne::create();
    _touchListener->retain();
    _touchListener->setSwallowTouches(true);
    // @005ec804 ($_0)
    _touchListener->onTouchBegan = [this](Touch* touch, Event* event) { return touchBegan(touch); };
    // @005ec8f8 ($_1)
    _touchListener->onTouchEnded = [this](Touch* touch, Event* event) { touchEnded(touch); };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(_touchListener, this);
}

// @005eafd0
void Mascot::setMascotState(MascotState state)
{
    if (_mascotState == state)
    {
        return;
    }
    _mascotState = state;

    if (state == MascotStateDefault)
    {
        if (_face == nullptr)
        {
            return;
        }
        int zOrder = _face->getLocalZOrder();
        _face->removeFromParentAndCleanup(false);
        _face = Sprite::createWithSpriteFrameName("mascot_default.png");
        addChild(_face, zOrder);
    }
    else if (state == MascotStateDead)
    {
        int zOrder = _face->getLocalZOrder();
        _face->removeFromParentAndCleanup(false);
        _face = Sprite::createWithSpriteFrameName("mascot_dead.png");
        addChild(_face, zOrder);
    }
    else if (state == MascotStateAnguished && _face != nullptr)
    {
        _hitCount++;
        if (_hitCount == _hitsToKill)
        {
            UserDefault* userDefault = UserDefault::getInstance();
            userDefault->setIntegerForKey(s_mascotStateKey.c_str(), MascotStateDead);
            userDefault->flush();
            Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "mascot_dead", "", -1);
            Settings::getInstance()->getSoundController()->playSound("LimbRip4");
            setMascotState(MascotStateDead);
        }
        else
        {
            Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "mascot_anguished", "", -1);

            int grunt = -1;
            do
            {
                grunt = cocos2d::random(1, _numGrunts);
            } while (grunt == _lastGruntIndex);
            _lastGruntIndex = grunt;

            std::string soundName = "mascot/grunt" + patch::to_string(grunt);
            _gruntAudioId = Settings::getInstance()->getSoundController()->playSound(soundName);
            float duration = experimental::AudioEngine::getDuration(_gruntAudioId);
            if (duration == -1.0f)
            {
                // Not known yet; update() keeps asking AudioEngine.
                duration = _defaultGruntDuration;
                _gruntDurationKnown = false;
            }
            _gruntTimeRemaining = duration;

            int zOrder = _face->getLocalZOrder();
            _face->removeFromParentAndCleanup(false);
            _face = Sprite::createWithSpriteFrameName("mascot_anguish.png");
            addChild(_face, zOrder);
        }
    }
}

// @005eb5cc
void Mascot::addFlair(MascotFlair flair)
{
    if (flair == MascotFlairTinyHat)
    {
        if (_flair != nullptr)
        {
            _flair->removeFromParentAndCleanup(false);
        }
        _flair = Sprite::createWithSpriteFrameName("mascot_tinyhat.png");
        _flair->setPosition(Vec2(0.0f, 15.0f));
    }
    else if (flair == MascotFlairMortarboard)
    {
        if (_flair != nullptr)
        {
            _flair->removeFromParentAndCleanup(false);
        }
        _flair = Sprite::createWithSpriteFrameName("mascot_mortorboard.png");
        _flair->setPosition(Vec2(3.0f, 22.0f));
    }
    else
    {
        return;
    }
    addChild(_flair);
}

// @005eb740
bool Mascot::touchBegan(Touch* touch)
{
    return _body->getTextureRect().containsPoint(_body->convertToNodeSpace(touch->getLocation()));
}

// @005eb7b4
void Mascot::touchEnded(Touch* touch)
{
    if (_mascotState == MascotStateDead)
    {
        Settings::getInstance()->getSoundController()->playSound("ShapeHit1");
    }
    else
    {
        setMascotState(MascotStateAnguished);
    }
}

// @005eb89c
void Mascot::update(float dt)
{
    _wavePhase = fmodf(_wavePhase + 0.05f, 6.28318548f);
    float waveRotation = sinf(_wavePhase) * 10.0f + 14.0f;

    if (_mascotState == MascotStateAnguished)
    {
        if (_gruntDurationKnown)
        {
            _gruntTimeRemaining = _gruntTimeRemaining - dt;
            float intensity = fmaxf(_gruntTimeRemaining / _gruntDuration, 0.0f);
            _legRight->setRotation(intensity * cocos2d::random(0, 20));
            _armRight->setRotation(waveRotation + (cocos2d::random(0, 20) + -10.0f) * intensity);
            _armLeft->setRotation(intensity * (cocos2d::random(0, 20) + -10.0f));
            _legLeft->setRotation(intensity * cocos2d::random(0, 20));
            setRotation(intensity * (cocos2d::random(0, 10) + -5.0f));
        }
        else
        {
            float duration = experimental::AudioEngine::getDuration(_gruntAudioId);
            if (duration != -1.0f)
            {
                _gruntDuration = duration;
                _gruntTimeRemaining = duration;
                _gruntDurationKnown = true;
            }
        }

        if (_gruntTimeRemaining <= 0.0f)
        {
            _legRight->setRotation(0.0f);
            _armLeft->setRotation(0.0f);
            _legLeft->setRotation(0.0f);
            setRotation(0.0f);
            _gruntDurationKnown = false;
            _gruntAudioId = -1;
            setMascotState(MascotStateDefault);
        }
    }
    else if (_mascotState == MascotStateDefault)
    {
        _armRight->setRotation(waveRotation);
    }
}

// @005ebb54
void Mascot::rewardedVideoComplete()
{
    switch (_rewardedVideoStatus)
    {
    case RewardedVideoStatusFailed:
    case RewardedVideoStatusNoConnection:
        _adFailedWindow = HWWindow::createAlertWindow("Failure", "Something went wrong. Please try again later.", "ok",
                                                      "", true, false, true);
        dismissAdRelatedWindows();
        break;

    case RewardedVideoStatusRewarded:
    {
        dismissAdRelatedWindows();
        Settings::getInstance()->getAdController()->setAdControllerDelegate(nullptr);

        UserDefault* userDefault = UserDefault::getInstance();
        userDefault->setIntegerForKey(s_mascotStateKey.c_str(), MascotStateDefault);
        int previousReviveCount = userDefault->getIntegerForKey(s_mascotReviveCountKey.c_str());
        int reviveCount = previousReviveCount + 1;
        userDefault->setIntegerForKey(s_mascotReviveCountKey.c_str(), reviveCount);
        if (previousReviveCount < 2)
        {
            HWWindow::createAlertWindow("necromancer level " + std::to_string(reviveCount),
                                        "thank you for reviving our beloved mascot.", "no problem", "", true, false,
                                        false);
        }
        else
        {
            if (!userDefault->getBoolForKey("nec_message_shown"))
            {
                std::string title = "necromancer level 3!";
                HWWindow::createAlertWindow(title, OW_GAMETEXT(mascotNecromancerLevel3Message, 0x410344),
                                            "i'm so proud", "", true, false, false);
                userDefault->setBoolForKey("nec_message_shown", true);
            }
            userDefault->setBoolForKey(s_necromancerKey.c_str(), true);
        }
        userDefault->flush();

        std::string label = "count_" + patch::to_string(reviveCount);
        Settings::getInstance()->getTracker()->submitAction(s_trackerCategory, "mascot_revived", label, -1);
        setMascotState(MascotStateDefault);
        if (previousReviveCount > 1)
        {
            addFlair(MascotFlairTinyHat);
        }
        _hitCount = 0;
        break;
    }

    default:
        break;
    }
}

// @005ec3b8
void Mascot::dismissAdRelatedWindows()
{
    if (_reviveWindow != nullptr)
    {
        _reviveWindow->dismissWindow(true);
        _reviveWindow = nullptr;
    }
    if (_loadingAdWindow != nullptr)
    {
        _loadingAdWindow->dismissWindow(true);
        _loadingAdWindow = nullptr;
    }
}

// @005ec3fc (thunk @005ec65c)
void Mascot::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    if (window == _reviveWindow)
    {
        if (buttonTag == 1)
        {
            _loadingAdWindow = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, this, false, false);
            _loadingAdWindow->showAlertMessage("Loading ad...", OW_GAMETEXT(mascotLoadingAdMessage, 0x41143e),
                                               "cancel", "", false);
            AdController* adController = Settings::getInstance()->getAdController();
            adController->setAdControllerDelegate(this);
            adController->showAd(AdTypeRewardedVideo);
        }
        else if (window != nullptr)
        {
            _reviveWindow = nullptr;
        }
    }
    else if (window == _loadingAdWindow)
    {
        window->dismissWindow(true);
    }
}

// @005ec664 (thunk @005ec668)
void Mascot::hwWindowWasDismissed(HWWindow* window)
{
}

// @005ec66c (thunk @005ec7a4)
void Mascot::rewardedVideoDidUpdateStatus(RewardedVideoStatus status)
{
    _rewardedVideoStatus = status;
    runAction(Sequence::create(DelayTime::create(1.0f),
                               CallFunc::create(CC_CALLBACK_0(Mascot::rewardedVideoComplete, this)), nullptr));
}
