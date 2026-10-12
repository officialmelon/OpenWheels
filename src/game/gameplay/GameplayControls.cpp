#include "GameplayControls.h"

#include "AdController.h"
#include "GameplayBtn.h"
#include "Globals.h"
#include "Settings.h"
#include "restored/Restored.h"  // RESTORED (PC addition)
#include "online/FlashRuntime.h"  // ONLINE (PC addition)
#include "online/vehicles/UserVehicle.h"  // ONLINE (PC addition)
#include "input/Gamepad.h"  // PAD (PC addition)
#include "qol/KeyBindings.h"       // QOL (PC addition)
#include "qol/QoL.h"               // QOL (PC addition)

USING_NS_CC;

// @005b4858
GameplayControls::GameplayControls()
    : _userScale(1.0f),
      _notchOffset(0.0f),
      _enabled(false),
      _hide(false),
      _unk0x32a(false),
      _levelComplete(false),
      _mode(ControlsModeDefault),
      _controlsType(ControlsTypeNone),
      _specialBtn(nullptr),
      _ejectBtn(nullptr),
      _meterBar(nullptr),
      _meterBG(nullptr),
      _timerBg(nullptr),
      _pauseBtnPos(),
      _resetBtnPos(),
      _cameraBtnPos(),
      _meterPos(),
      _forwardPos(),
      _backwardPos(),
      _leanForwardPos(),
      _leanBackwardPos(),
      _specialPos(),
      _ejectPos(),
      _timerBgPos(),
      _buttons(),
      _pauseBtn(nullptr),
      _resetBtn(nullptr),
      _touchListener(nullptr),
      _characterEjectedListener(nullptr),
      _characterDeadListener(nullptr),
      _drawBounds(false),
      _replayIndicator(nullptr),
      _replayIndicatorTouch(nullptr),
      _unk0x448(false)
{
    // _originalMeterY/_originalMeterHeight, the banner listeners and the layout metrics are left
    // uninitialised here (init() sets them), as in the original.
}

// @005b48dc (D1), @005b49d8 (D0)
GameplayControls::~GameplayControls()
{
    _eventDispatcher->removeEventListener(_touchListener);
    _touchListener->release();
    _touchListener = nullptr;
    getEventDispatcher()->removeEventListener(_characterEjectedListener);
    _characterEjectedListener->release();
    _characterEjectedListener = nullptr;
    getEventDispatcher()->removeEventListener(_characterDeadListener);
    _characterDeadListener->release();
    _characterDeadListener = nullptr;
    if (_bannerShownListener)
    {
        getEventDispatcher()->removeEventListener(_bannerShownListener);
        _bannerShownListener->release();
        _bannerShownListener = nullptr;
    }
    if (_bannerRemovedListener)
    {
        getEventDispatcher()->removeEventListener(_bannerRemovedListener);
        _bannerRemovedListener->release();
        _bannerRemovedListener = nullptr;
    }
}

// @005b49fc
bool GameplayControls::init()
{
    bool result = Layer::init();
    if (result)
    {
        UserDefault* userDefault = UserDefault::getInstance();
        _notchOffset = userDefault->getIntegerForKey("adjust_controls_for_notch") * 180.0f;
        int userScale = userDefault->getIntegerForKey("controls_user_scale");
        _controlsType = ControlsTypeNone;
        _meterBar = nullptr;
        _timerBg = nullptr;
        _meterBG = nullptr;
        _bannerRemovedListener = nullptr;
        _userScale = userScale * 0.25f + 1.0f;
        _dpadArmLength = 225.0f * _userScale;
        _dpadCenterX = (_notchOffset + 450.0f) * _userScale;
        _dpadCenterY = 450.0f * _userScale;
        _bottomMargin = 100.0f * _userScale;
        _sideMargin = (_notchOffset + 60.0f) * _userScale;
        _topMargin = 60.0f * _userScale;
        _buttonSpacing = 90.0f * _userScale;
        _specialButtonSpacing = 125.0f * _userScale;
        _touchListener = nullptr;
        _characterEjectedListener = nullptr;
        _characterDeadListener = nullptr;
        _bannerShownListener = nullptr;
        // both fetched but not used
        Size visibleSize = Director::getInstance()->getVisibleSize();
        Vec2 origin = Director::getInstance()->getVisibleOrigin();
        addSprites();
        addEvents();
        _enabled = true;
    }
    return result;
}

// @005b4b54
void GameplayControls::addSprites()
{
    SpriteFrameCache* cache = SpriteFrameCache::getInstance();
    if (!cache->isSpriteFramesWithFileLoaded("controls/gameplay/controls_gameplay.plist"))
    {
        cache->addSpriteFramesWithFile("controls/gameplay/controls_gameplay.plist");
    }
}

// @005b4c28
void GameplayControls::addEvents()
{
    _touchListener = EventListenerTouchOneByOne::create();
    _touchListener->retain();
    _touchListener->setSwallowTouches(true);
    // @005b89d4 ($_0)
    _touchListener->onTouchBegan = [this](Touch* touch, Event* event) { return touchBegan(touch); };
    // @005b8a5c ($_1)
    _touchListener->onTouchMoved = [this](Touch* touch, Event* event) { touchMoved(touch); };
    // @005b8ae4 ($_2)
    _touchListener->onTouchEnded = [this](Touch* touch, Event* event) { touchEnded(touch); };
    // @005b8b6c ($_3) cancelled touches are handled as ended ones
    _touchListener->onTouchCancelled = [this](Touch* touch, Event* event) { touchEnded(touch); };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(_touchListener, 1);

    // @005b8bf4 ($_4)
    _characterEjectedListener = getEventDispatcher()->addCustomEventListener(
        "characterEjected", [this](EventCustom* event) { addControls(ControlsTypeEjected); });
    _characterEjectedListener->retain();
    // @005b8c7c ($_5)
    _characterDeadListener = getEventDispatcher()->addCustomEventListener(
        "characterDead", [this](EventCustom* event) { handleDeath(); });
    _characterDeadListener->retain();
    // @005b8d00 ($_6) RE-TODO(@005b8d00): the body is only AdController::getBannerAdSize(), which
    // is what bannerAdShown/bannerRemoved/updateUpperUIToAccommodateBanner all reduce to; which of
    // them the lambdas called is not recoverable.
    _bannerShownListener = getEventDispatcher()->addCustomEventListener(
        "banner_shown", [this](EventCustom* event) { updateUpperUIToAccommodateBanner(); });
    _bannerShownListener->retain();
    // @005b8dcc ($_7)
    _bannerRemovedListener = getEventDispatcher()->addCustomEventListener(
        "banner_removed", [this](EventCustom* event) { bannerRemoved(); });
    _bannerRemovedListener->retain();
}

// @005b50e8
GameplayControls* GameplayControls::createWithControlsType(ControlsType type)
{
    GameplayControls* controls = new GameplayControls();
    if (!controls->init())
    {
        delete controls;
        return nullptr;
    }
    controls->autorelease();
    if (type != ControlsTypeNone)
    {
        controls->addControls(type);
    }
    controls->addPauseBtn();
    return controls;
}

// @005b5178
void GameplayControls::addControls(ControlsType type)
{
    if (_levelComplete)
    {
        return;
    }
    if (_controlsType == type)
    {
        return;
    }
    for (GameplayBtn* btn : _buttons)
    {
        btn->removeFromParent();
    }
    _buttons.clear();

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    switch (type)
    {
    case ControlsTypeEjected:
    {
        GameplayBtn* grabBtn = GameplayBtn::createWithSpriteFrameName(
            "controls_gameplay_btn_grab.png", GameplayControlsStateGrab, _userScale);
        grabBtn->setAdjustedScale(1.0f);
        Size grabSize = grabBtn->getContentSize();
        grabBtn->setPosition(
            Vec2(origin.x + visibleSize.width - grabSize.width * 0.5f - _sideMargin,
                 origin.y + _bottomMargin + grabSize.height * 0.5f));
        grabBtn->setHitArea(Rect(Rect::ZERO));
        grabBtn->nudgeBounds(60.0f, 180.0f, 120.0f, 36.0f);
        addChild(grabBtn);
        _buttons.push_back(grabBtn);

        GameplayBtn* supermanBtn = GameplayBtn::createWithSpriteFrameName(
            "controls_gameplay_btn_superman.png", GameplayControlsStateSuperman, _userScale);
        supermanBtn->setAdjustedScale(1.0f);
        supermanBtn->setPosition(Vec2(origin.x + _dpadCenterX,
                                      origin.y + _dpadCenterY + _dpadArmLength));
        supermanBtn->setHitArea(Rect(Rect::ZERO));
        supermanBtn->nudgeBounds(90.0f, 18.0f, 6.0f, 18.0f);
        addChild(supermanBtn);
        _buttons.push_back(supermanBtn);

        GameplayBtn* tuckBtn = GameplayBtn::createWithSpriteFrameName(
            "controls_gameplay_btn_tuck.png", GameplayControlsStateTuck, _userScale);
        tuckBtn->setAdjustedScale(1.0f);
        tuckBtn->setPosition(Vec2(origin.x + _dpadCenterX,
                                  origin.y + _dpadCenterY - _dpadArmLength));
        tuckBtn->setHitArea(Rect(Rect::ZERO));
        tuckBtn->nudgeBounds(6.0f, 18.0f, 90.0f, 18.0f);
        addChild(tuckBtn);
        _buttons.push_back(tuckBtn);

        GameplayBtn* archBtn = GameplayBtn::createWithSpriteFrameName(
            "controls_gameplay_btn_arch.png", GameplayControlsStateArch, _userScale);
        archBtn->setAdjustedScale(1.0f);
        archBtn->setPosition(Vec2(origin.x + _dpadCenterX - _dpadArmLength,
                                  origin.y + _dpadCenterY));
        archBtn->setHitArea(Rect(Rect::ZERO));
        archBtn->nudgeBounds(18.0f, 6.0f, 18.0f, 90.0f);
        addChild(archBtn);
        _buttons.push_back(archBtn);

        GameplayBtn* pushupBtn = GameplayBtn::createWithSpriteFrameName(
            "controls_gameplay_btn_pushup.png", GameplayControlsStatePushup, _userScale);
        pushupBtn->setAdjustedScale(1.0f);
        pushupBtn->setPosition(Vec2(origin.x + _dpadCenterX + _dpadArmLength,
                                    origin.y + _dpadCenterY));
        pushupBtn->setHitArea(Rect(Rect::ZERO));
        pushupBtn->nudgeBounds(18.0f, 90.0f, 18.0f, 6.0f);
        addChild(pushupBtn);
        _buttons.push_back(pushupBtn);
        // RESTORED (PC addition): extra buttons of the restored characters' control sets.
        for (GameplayBtn* btn : restored::extraControls(
                 Settings::getInstance()->getSelectedCharacterControlType(), true, _userScale,
                 Vec2::ZERO, Vec2::ZERO, 0.0f, grabBtn->getPosition(), grabSize.height,
                 _specialButtonSpacing))
        {
            addChild(btn);
            _buttons.push_back(btn);
        }
        // ONLINE (PC addition): a rider of a browser user vehicle uses this layout.
        if (online::flashLevel())
        {
            onlineAddUserVehicleButtons(grabBtn->getPosition(), grabSize);
        }

        if (_drawBounds)
        {
            DrawNode* boundsNode = DrawNode::create(2.0f);
            addChild(boundsNode, 9999);
            Rect hitArea = grabBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
            hitArea = supermanBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
            hitArea = tuckBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
            hitArea = archBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
            hitArea = pushupBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
        }
        break;
    }

    case ControlsTypeDead:
        if (_pauseBtn)
        {
            _pauseBtn->setEnabled(false);
            _pauseBtn->setPosition(
                Vec2(_pauseBtnPos.x,
                     _pauseBtnPos.y -
                         Settings::getInstance()->getAdController()->getBannerAdSize().height));
        }
        break;

    case ControlsTypeDeadReplay:
        if (_pauseBtn)
        {
            _pauseBtn->setEnabled(true);
        }
        break;

    case ControlsTypeVictory:
        if (_mode != ControlsModeTesting && _pauseBtn)
        {
            _pauseBtn->removeFromParentAndCleanup(true);
            _pauseBtn = nullptr;
        }
        _timerBg->setVisible(false);
        break;

    default:
    {
        // forward (bottom right)
        GameplayBtn* forwardBtn = GameplayBtn::createWithSpriteFrameName(
            "controls_gameplay_btn_move.png", GameplayControlsStateForward, _userScale);
        forwardBtn->setAdjustedScale(1.0f);
        Size forwardSize = forwardBtn->getContentSize();
        _forwardPos.x = origin.x + visibleSize.width - _sideMargin - forwardSize.width * 0.5f;
        _forwardPos.y = origin.y + _bottomMargin + forwardSize.height * 0.5f;
        forwardBtn->setPosition(_forwardPos);
        forwardBtn->setHitArea(Rect(Rect::ZERO));
        forwardBtn->nudgeBounds(60.0f, 180.0f, 120.0f, 36.0f);
        addChild(forwardBtn);
        _buttons.push_back(forwardBtn);

        // backward (left of forward)
        GameplayBtn* backwardBtn = GameplayBtn::createWithSpriteFrameName(
            "controls_gameplay_btn_move.png", GameplayControlsStateBackward, _userScale);
        backwardBtn->setAdjustedScale(1.0f);
        Size backwardSize = backwardBtn->getContentSize();
        _backwardPos.x = _forwardPos.x - forwardSize.width * 0.5f - _buttonSpacing -
                         backwardSize.width * 0.5f;
        _backwardPos.y = origin.y + _bottomMargin + backwardSize.height * 0.5f;
        backwardBtn->setPosition(_backwardPos);
        backwardBtn->setHitArea(Rect(Rect::ZERO));
        backwardBtn->nudgeBounds(60.0f, 36.0f, 120.0f, 180.0f);
        backwardBtn->setFlippedX(true);
        addChild(backwardBtn);
        _buttons.push_back(backwardBtn);

        // lean back (bottom left)
        GameplayBtn* leanBackwardBtn = GameplayBtn::createWithSpriteFrameName(
            "controls_gameplay_btn_lean.png", GameplayControlsStateLeanBackward, _userScale);
        leanBackwardBtn->setAdjustedScale(1.0f);
        Size leanBackwardSize = leanBackwardBtn->getContentSize();
        _leanBackwardPos = Vec2(origin.x + _sideMargin + leanBackwardSize.width * 0.5f,
                                origin.y + _bottomMargin + leanBackwardSize.height * 0.5f);
        leanBackwardBtn->setPosition(_leanBackwardPos);
        leanBackwardBtn->setHitArea(Rect(Rect::ZERO));
        leanBackwardBtn->nudgeBounds(60.0f, 36.0f, 120.0f, 180.0f);
        leanBackwardBtn->setFlippedX(true);
        addChild(leanBackwardBtn);
        _buttons.push_back(leanBackwardBtn);

        // lean forward (right of lean back)
        GameplayBtn* leanForwardBtn = GameplayBtn::createWithSpriteFrameName(
            "controls_gameplay_btn_lean.png", GameplayControlsStateLeanForward, _userScale);
        leanForwardBtn->setAdjustedScale(1.0f);
        Size leanForwardSize = leanForwardBtn->getContentSize();
        _leanForwardPos.x = _leanBackwardPos.x + leanBackwardSize.width * 0.5f + _buttonSpacing +
                            leanForwardSize.width * 0.5f;
        _leanForwardPos.y = origin.y + _bottomMargin + leanForwardSize.height * 0.5f;
        leanForwardBtn->setPosition(_leanForwardPos);
        leanForwardBtn->setHitArea(Rect(Rect::ZERO));
        leanForwardBtn->nudgeBounds(60.0f, 180.0f, 120.0f, 36.0f);
        addChild(leanForwardBtn);
        _buttons.push_back(leanForwardBtn);

        // special (above the move or the lean buttons)
        _specialPos = Vec2::ZERO;
        int overrideSpecialPosition =
            UserDefault::getInstance()->getIntegerForKey("override_special_position");
        bool specialOnLeft;
        if (overrideSpecialPosition == 1)
        {
            specialOnLeft = false;
        }
        else
        {
            specialOnLeft = type == ControlsTypeBusinessGuy || type == ControlsTypePogoStick ||
                            overrideSpecialPosition == 2;
        }
        std::string specialFrame;
        switch (type)
        {
        case ControlsTypeBusinessGuy:
            specialFrame = "controls_gameplay_btn_jump.png";
            break;
        case ControlsTypeIrresponsibleDad:
            specialFrame = "controls_gameplay_btn_break.png";
            break;
        case ControlsTypeWheelchairGuy:
            specialFrame = "controls_gameplay_btn_jet.png";
            break;
        case ControlsTypeMoped:
            specialFrame = "controls_gameplay_btn_jet.png";
            break;
        case ControlsTypePogoStick:
            specialFrame = "controls_gameplay_btn_jump.png";
            break;
        default:
            specialFrame = "controls_gameplay_btn_jump.png";
            break;
        }
        // RESTORED (PC addition): the restored characters' special button (art and side).
        restored::controlsSpecial(type, overrideSpecialPosition, &specialFrame, &specialOnLeft);
        _specialBtn = GameplayBtn::createWithSpriteFrameName(specialFrame, GameplayControlsStateSpecial,
                                                             _userScale);
        _specialBtn->setAdjustedScale(1.0f);
        Size specialSize = _specialBtn->getContentSize();
        if (specialOnLeft)
        {
            _specialPos.x = origin.x + _sideMargin + specialSize.width * 0.5f;
            _specialPos.y = _leanBackwardPos.y + leanForwardSize.height * 0.5f +
                            _specialButtonSpacing + specialSize.height * 0.5f;
        }
        else
        {
            _specialPos.x =
                origin.x + visibleSize.width - _sideMargin + specialSize.width * -0.5f;
            _specialPos.y = _forwardPos.y + leanForwardSize.height * 0.5f + _specialButtonSpacing +
                            specialSize.height * 0.5f;
        }
        _specialBtn->setPosition(_specialPos);
        _specialBtn->setHitArea(Rect(Rect::ZERO));
        _specialBtn->nudgeBounds(240.0f, 156.0f, 36.0f, 156.0f);
        addChild(_specialBtn);
        _buttons.push_back(_specialBtn);

        // pause (top left; added by addPauseBtn) and eject (top right)
        _pauseBtnPos.x = origin.x + _sideMargin + specialSize.width * 0.5f * _userScale;
        _pauseBtnPos.y = origin.y + visibleSize.height - _topMargin - specialSize.height * 0.5f;
        _ejectBtn = GameplayBtn::createWithSpriteFrameName("controls_gameplay_btn_eject.png",
                                                           GameplayControlsStateEject, _userScale);
        _ejectBtn->setAdjustedScale(1.0f);
        Size ejectSize = _ejectBtn->getContentSize();
        _ejectPos = Vec2(origin.x + visibleSize.width - _sideMargin + ejectSize.width * -0.5f,
                         origin.y + visibleSize.height - _topMargin + ejectSize.height * -0.5f);
        _ejectBtn->setPosition(_ejectPos);
        _ejectBtn->setHitArea(Rect(Rect::ZERO));
        _ejectBtn->nudgeBounds(180.0f, 180.0f, 96.0f, 96.0f);
        addChild(_ejectBtn);
        _buttons.push_back(_ejectBtn);
        // RESTORED (PC addition): extra buttons of the restored characters' control sets.
        for (GameplayBtn* btn : restored::extraControls(type, false, _userScale, _leanBackwardPos,
                                                        _leanForwardPos, leanForwardSize.height,
                                                        Vec2::ZERO, 0.0f, _specialButtonSpacing))
        {
            // They sit above the lean buttons, where a special button on the left goes too (the
            // Options' special position, Lawn Mower Man): move them above it, or the special's
            // enlarged hit area takes their touches and the keyboard's fingers.
            if (specialOnLeft)
            {
                btn->setPosition(btn->getPosition() + Vec2(0.0f, specialSize.height + _specialButtonSpacing + 280.0f));  // clear of its hit area (+240 top)
                btn->setHitArea(Rect(Rect::ZERO));
                btn->nudgeBounds(36.0f, 36.0f, 36.0f, 36.0f);
            }
            addChild(btn);
            _buttons.push_back(btn);
        }

        if (_drawBounds)
        {
            DrawNode* boundsNode = DrawNode::create(2.0f);
            addChild(boundsNode, 9999);
            Rect hitArea = forwardBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
            hitArea = backwardBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
            hitArea = leanForwardBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
            hitArea = leanBackwardBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
            hitArea = _specialBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
            hitArea = _ejectBtn->getHitArea();
            boundsNode->drawRect(hitArea.origin,
                                 Vec2(hitArea.origin.x + hitArea.size.width,
                                      hitArea.origin.y + hitArea.size.height),
                                 Color4F(1.0f, 0.0f, 0.0f, 1.0f));
        }

        // moped boost meter, left of the special button
        // RESTORED (PC addition): also for the restored characters with a boost (Santa's flight).
        if (type == ControlsTypeMoped || restored::controlsMeter(type))
        {
            _meterBG = Sprite::createWithSpriteFrameName("controls_meter_back.png");
            Size meterBGSize = _meterBG->getContentSize();
            Vec2 meterPosition(_specialPos.x + specialSize.width * 0.5f - meterBGSize.width,
                               _specialPos.y + (_ejectPos.y - _specialPos.y) * 0.5f);
            _meterBG->setPosition(meterPosition);
            addChild(_meterBG);
            _meterBar = Sprite::createWithSpriteFrameName("controls_meter_bar.png");
            Size meterBarSize = _meterBar->getContentSize();
            _meterBar->setAnchorPoint(Vec2(0.5f, 0.0f));
            _meterBar->setPosition(
                Vec2(meterPosition.x, meterPosition.y + meterBarSize.height * -0.5f));
            addChild(_meterBar);
            _originalMeterHeight = _meterBar->getTextureRect().size.height;
            _originalMeterY = _meterBar->isTextureRectRotated() ? _meterBar->getTextureRect().origin.x
                                                                : _meterBar->getTextureRect().origin.y;
        }
        break;
    }
    }

    // QOL (PC addition): touch controls hidden (desktop default): the state buttons stay for the
    // keyboard bridge but are not drawn and ignore real touches.
    if (!qol::touchControlsShown())
    {
        for (GameplayBtn* btn : _buttons)
        {
            btn->setKeyOnly(true);
        }
    }

    updateUpperUIToAccommodateBanner();  // inlined in the original
}

// @005b738c
void GameplayControls::addPauseBtn()
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    _pauseBtn = GameplayBtn::createWithSpriteFrameName("controls_gameplay_btn_pause.png", _pauseBtnPos,
                                                       Rect(), 0, _userScale);
    _pauseBtn->setAdjustedScale(1.0f);
    _pauseBtn->setTag(ControlsLayerActionPause);
    Size pauseSize = _pauseBtn->getContentSize();  // unused
    _pauseBtn->nudgeBounds(14.0f, 14.0f, 14.0f, 14.0f);
    addChild(_pauseBtn);

    if (!_timerBg)
    {
        Size size = _pauseBtn->getContentSize();
        float scale = _pauseBtn->getScale();
        _timerBg = Sprite::createWithSpriteFrameName("controls_timer_bg.png");
        _timerBgPos.x = origin.x + visibleSize.width * 0.5f;
        _timerBgPos.y = _pauseBtnPos.y + size.height * 0.5f * scale -
                        _timerBg->getContentSize().height * 0.5f;
        _timerBg->setPosition(_timerBgPos);
        addChild(_timerBg);
    }
}

// @005b75ec
void GameplayControls::updateUpperUIToAccommodateBanner()
{
    Settings::getInstance()->getAdController()->getBannerAdSize();
}

// @005b763c
void GameplayControls::addResetBtn()
{
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();
    _resetBtn = GameplayBtn::createWithSpriteFrameName("controls_gameplay_btn_reset.png", Vec2::ZERO,
                                                       Rect(), 0, _userScale);
    _resetBtn->setAdjustedScale(1.0f);
    _resetBtn->setTag(ControlsLayerActionReset);
    Size size = _resetBtn->getContentSize();
    _resetBtnPos.x = origin.x + visibleSize.width * 0.5f;
    _resetBtnPos.y = origin.y + _bottomMargin + size.height * 0.5f;
    _resetBtn->setPosition(_resetBtnPos);
    _resetBtn->nudgeBounds(14.0f, 14.0f, 14.0f, 14.0f);
    addChild(_resetBtn);
    // QOL (PC addition): desktop builds show the restart key above the button (scales with it);
    // PAD (PC addition): so does any build with a game controller connected (its button).
    if (qol::desktopBuild() || openwheels::pad::anyConnected())
    {
        Label* hint = qol::createKeyHintLabel(qol::KeyAction::Restart, 70.0f);
        hint->setAnchorPoint(Vec2(0.5f, 0.0f));
        hint->setPosition(Vec2(size.width * 0.5f, size.height + 12.0f));
        _resetBtn->addChild(hint);
    }
}

// @005b77ec
void GameplayControls::handleDeath()
{
    if (_mode == ControlsModeReplay)
    {
        addControls(ControlsTypeDeadReplay);
        removeMeterBar();
    }
    else
    {
        addControls(ControlsTypeDead);
        removeMeterBar();
        float delay = globals::ui::postDeathDelayToAllowAdToLoadOrRegisterImpression;
        // QOL (PC addition): no ad to wait for on desktop: pause / reset (and their keys) come
        // back after a short beat instead of 3 s.
        if (qol::desktopBuild())
        {
            delay = 0.5f;
        }
        runAction(Sequence::create(
            DelayTime::create(delay),
            CallFunc::create(CC_CALLBACK_0(GameplayControls::addPostDeathControls, this)),
            nullptr));
    }
}

// @005b79cc
void GameplayControls::removeMeterBar()
{
    if (_meterBG)
    {
        _meterBar->removeFromParentAndCleanup(false);
        _meterBG->removeFromParentAndCleanup(false);
        _meterBar = nullptr;
        _meterBG = nullptr;
    }
}

// @005b7a28
void GameplayControls::addPostDeathControls()
{
    if (_levelComplete)
    {
        return;
    }
    addResetBtn();
    if (_resetBtn)
    {
        float scale = _resetBtn->getScale();
        _resetBtn->setScale(0.0f);
        _resetBtn->runAction(EaseBounceOut::create(ScaleTo::create(0.4f, scale)));
    }
    if (_pauseBtn)
    {
        _pauseBtn->setEnabled(true);
        float scale = _pauseBtn->getScale();
        _pauseBtn->setScale(0.0f);
        _pauseBtn->runAction(EaseBounceOut::create(ScaleTo::create(0.4f, scale)));
    }
}

// @005b7b24
Sprite* GameplayControls::getTimerBg()
{
    return _timerBg;
}

// @005b7b2c
int GameplayControls::getState()
{
    int state = 0;
    for (GameplayBtn* btn : _buttons)
    {
        if (btn->getTouch())
        {
            state += btn->getStateValue();
        }
    }
    return state;
}

// @005b7ba0
void GameplayControls::setState(unsigned char state)
{
    for (GameplayBtn* btn : _buttons)
    {
        btn->showPressedState((btn->getStateValue() & state) != 0);
    }
}

// @005b7c00
void GameplayControls::victory()
{
    addControls(ControlsTypeVictory);
    _levelComplete = true;
}

// @005b7c2c
bool GameplayControls::touchBegan(Touch* touch)
{
    Vec2 location = touch->getLocation();
    if (_pauseBtn && _pauseBtn->getEnabled())
    {
        if (_pauseBtn->getTouch())
        {
            return true;
        }
        if (_pauseBtn->getHitArea().containsPoint(location))
        {
            _pauseBtn->setTouch(touch);
            return true;
        }
    }
    if (_resetBtn && _resetBtn->getEnabled())
    {
        if (_resetBtn->getTouch())
        {
            return true;
        }
        if (_resetBtn->getHitArea().containsPoint(location))
        {
            _resetBtn->setTouch(touch);
            return true;
        }
    }
    if (_mode == ControlsModeReplay)
    {
        // replays only show a "REPLAY MODE" overlay while the screen is touched
        if (_replayIndicator)
        {
            return false;
        }
        _replayIndicatorTouch = touch;
        addReplayIndicator();
        return true;
    }
    for (GameplayBtn* btn : _buttons)
    {
        // QOL (PC addition): hidden touch controls only take the keyboard bridge's fingers.
        if (btn->getKeyOnly() && !qol::keyboardTouch())
        {
            continue;
        }
        // ONLINE (PC addition): the user-vehicle buttons are hidden while not riding one (every
        // original button is always visible).
        if (!btn->isVisible())
        {
            continue;
        }
        if (!btn->getTouch() && btn->getHitArea().containsPoint(location))
        {
            btn->setTouch(touch);
            return true;
        }
    }
    return false;
}

// @005b7d88
void GameplayControls::addReplayIndicator()
{
    _replayIndicator = LayerColor::create(Color4B(0, 0, 0, 90));
    std::string text = "REPLAY MODE";
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();  // unused
    Label* label = Label::createWithTTF(text, "fonts/ClarendonLTStd-Bold.ttf", 100.0f, Size::ZERO,
                                        TextHAlignment::LEFT, TextVAlignment::TOP);
    label->setAlignment(TextHAlignment::CENTER);
    label->setColor(globals::colors::yellow);
    label->setAnchorPoint(Vec2(0.5f, 0.5f));
    label->setPosition(Vec2(visibleSize.width * 0.5f, visibleSize.height * 0.5f));
    _replayIndicator->addChild(label);
    if (_pauseBtn)
    {
        Sprite* ring = Sprite::createWithSpriteFrameName("controls_ring.png");
        ring->setPosition(_pauseBtn->getPosition());
        _replayIndicator->addChild(ring);
        addAnimationToRing(ring);
    }
    addChild(_replayIndicator);
}

// @005b80c0
void GameplayControls::touchMoved(Touch* touch)
{
    Vec2 location = touch->getLocation();

    // release whatever this touch holds once it leaves that button
    for (GameplayBtn* btn : _buttons)
    {
        if (btn->getTouch() == touch)
        {
            if (btn->getHitArea().containsPoint(location))
            {
                return;
            }
            btn->setTouch(nullptr);
            break;
        }
    }
    if (_pauseBtn && _pauseBtn->getTouch() == touch)
    {
        if (_pauseBtn->getHitArea().containsPoint(location))
        {
            return;
        }
        _pauseBtn->setTouch(nullptr);
    }
    if (_resetBtn && _resetBtn->getTouch() == touch)
    {
        if (_resetBtn->getHitArea().containsPoint(location))
        {
            return;
        }
        _resetBtn->setTouch(nullptr);
    }

    // pick up the button it moved onto
    for (GameplayBtn* btn : _buttons)
    {
        // QOL (PC addition): hidden touch controls are not picked up by a dragged touch (the
        // keyboard bridge's fingers never move).
        if (btn->getKeyOnly())
        {
            continue;
        }
        if (btn->getHitArea().containsPoint(location))
        {
            btn->setTouch(touch);
            return;
        }
    }
    if (_pauseBtn)
    {
        if (_pauseBtn->getTouch())
        {
            return;
        }
        if (_pauseBtn->getHitArea().containsPoint(location))
        {
            _pauseBtn->setTouch(touch);
            return;
        }
    }
    if (_resetBtn)
    {
        if (_resetBtn->getTouch())
        {
            return;
        }
        if (_resetBtn->getHitArea().containsPoint(location))
        {
            _resetBtn->setTouch(touch);
        }
    }
}

// @005b82a4
void GameplayControls::touchEnded(Touch* touch)
{
    if (_mode == ControlsModeReplay)
    {
        if (_replayIndicatorTouch == touch)
        {
            _replayIndicatorTouch = nullptr;
            removeReplayIndicator();
        }
    }
    else
    {
        for (GameplayBtn* btn : _buttons)
        {
            if (btn->getTouch() == touch)
            {
                btn->setTouch(nullptr);
                return;
            }
        }
    }

    Vec2 location = touch->getLocation();
    if (_pauseBtn && _pauseBtn->getHitArea().containsPoint(location))
    {
        _pauseBtn->setTouch(nullptr);
        int action = _pauseBtn->getTag();
        getEventDispatcher()->dispatchCustomEvent("gameplayControlsAction", &action);
    }
    if (_resetBtn && _resetBtn->getHitArea().containsPoint(location))
    {
        _resetBtn->setTouch(nullptr);
        int action = _resetBtn->getTag();
        getEventDispatcher()->dispatchCustomEvent("gameplayControlsAction", &action);
    }
}

// @005b84e8
void GameplayControls::removeReplayIndicator()
{
    _replayIndicator->removeFromParent();
    _replayIndicator = nullptr;
}

// @005b8518
void GameplayControls::touchCancelled(Touch* touch)
{
    touchEnded(touch);
}

// @005b851c
void GameplayControls::setHidden(bool hidden)
{
    if (_hide == hidden)
    {
        return;
    }
    _hide = hidden;
    if (hidden)
    {
        for (GameplayBtn* btn : _buttons)
        {
            btn->setTouch(nullptr);
        }
        setEnabled(false);
    }
    else
    {
        setEnabled(true);
    }
    setVisible(!hidden);
}

// @005b85ec
void GameplayControls::setEnabled(bool enabled)
{
    if (_enabled == enabled)
    {
        return;
    }
    if (enabled)
    {
        Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(
            _touchListener, 1);
    }
    else
    {
        Director::getInstance()->getEventDispatcher()->removeEventListener(_touchListener);
    }
    _enabled = enabled;
}

// @005b8648
ControlsMode GameplayControls::getMode()
{
    return _mode;
}

// @005b8650
void GameplayControls::setMode(ControlsMode mode)
{
    _mode = mode;
}

// @005b8658
void GameplayControls::setMeterPercentage(float percentage)
{
    if (!_meterBar)
    {
        return;
    }
    bool rotated = _meterBar->isTextureRectRotated();
    Rect rect = _meterBar->getTextureRect();
    rect.size.height = _originalMeterHeight * percentage;
    float y = _originalMeterHeight - rect.size.height + _originalMeterY;
    if (rotated)
    {
        rect.origin.x = y;
    }
    else
    {
        rect.origin.y = y;
    }
    _meterBar->setTextureRect(rect, rotated, rect.size);
}

// @005b8714
void GameplayControls::addAnimationToRing(Sprite* ring)
{
    ring->stopAllActions();
    ring->setVisible(true);
    ring->setScale(1.5f);
    ring->setOpacity(0);
    ActionInterval* scale = ScaleTo::create(0.5f, 0.85f);
    ActionInterval* fade = FadeIn::create(0.5f);
    ring->runAction(EaseExponentialOut::create(scale));
    ring->runAction(fade);
}

// @005b87cc
void GameplayControls::bannerAdShown(int height)
{
    Settings::getInstance()->getAdController()->getBannerAdSize();
}

// @005b881c
void GameplayControls::bannerRemoved()
{
    Settings::getInstance()->getAdController()->getBannerAdSize();
}

// @005b886c
Vec2 GameplayControls::getForwardPos()
{
    return _forwardPos;
}

// @005b8878
Vec2 GameplayControls::getBackwardPos()
{
    return _backwardPos;
}

// @005b8884
Vec2 GameplayControls::getLeanForwardPos()
{
    return _leanForwardPos;
}

// @005b8890
Vec2 GameplayControls::getLeanBackwardPos()
{
    return _leanBackwardPos;
}

// @005b889c
Vec2 GameplayControls::getSpecialPos()
{
    return _specialPos;
}

// @005b88a8
Vec2 GameplayControls::getEjectPos()
{
    return _ejectPos;
}

// ONLINE (PC addition): Flash's shift / ctrl (a user vehicle's assigned actions: brake, jets,
// arrow guns) and Z (eject) have no button in the mobile ejected layout a rider uses, so touch
// and mouse players could not use them (the keyboard sends them directly, see
// online::pcExtraControlBits). Two buttons left of the grab button, labelled with the keys
// level authors name in their instructions, and the eject button at its usual place; each shows
// while the main character rides a vehicle that uses it.
void GameplayControls::onlineAddUserVehicleButtons(const Vec2& grabPos, const Size& grabSize)
{
    const Size visibleSize = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();
    const char* const frames[3] = {"controls_gameplay_btn_jet.png", "controls_gameplay_btn_jet.png",
                                   "controls_gameplay_btn_eject.png"};
    const unsigned int bits[3] = {0x20, 0x40, 0x80};
    const char* const labels[3] = {"SHIFT", "CTRL", nullptr};
    float x = grabPos.x - grabSize.width * 0.5f;
    for (int i = 0; i < 3; i++)
    {
        GameplayBtn* btn = GameplayBtn::createWithSpriteFrameName(frames[i], bits[i], _userScale);
        if (!btn)
        {
            continue;
        }
        btn->setAdjustedScale(1.0f);
        const Size size = btn->getContentSize();
        if (i < 2)
        {
            x -= _specialButtonSpacing + size.width * 0.5f;
            btn->setPosition(Vec2(x, grabPos.y));
            x -= size.width * 0.5f;
        }
        else
        {
            btn->setPosition(Vec2(origin.x + visibleSize.width - _sideMargin - size.width * 0.5f,
                                  origin.y + visibleSize.height - _topMargin - size.height * 0.5f));
        }
        btn->setHitArea(Rect(Rect::ZERO));
        btn->nudgeBounds(24.0f, 24.0f, 24.0f, 24.0f);
        if (labels[i] && qol::touchControlsShown())
        {
            Label* label = Label::createWithTTF(labels[i], "fonts/ClarendonLTStd-Bold.ttf", 56.0f);
            label->setColor(globals::colors::yellow);
            label->enableOutline(Color4B(0, 0, 0, 255), 5);
            label->setPosition(Vec2(size.width * 0.5f, size.height * 0.5f));
            btn->addChild(label);
        }
        btn->setVisible(false);
        btn->schedule(
            [btn, i](float) {
                online::UserVehicle* vehicle = online::riddenUserVehicle();
                bool show = vehicle != nullptr;
                if (show && i == 0) show = vehicle->getShiftAction() != online::UserVehicle::ActionNone;
                if (show && i == 1) show = vehicle->getCtrlAction() != online::UserVehicle::ActionNone;
                if (btn->isVisible() != show)
                {
                    btn->setVisible(show);
                }
            },
            "ow_user_vehicle_button");
        addChild(btn);
        _buttons.push_back(btn);
    }
}
