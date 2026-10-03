#include "ButtonWithBatchedSprite.h"

#include "cocos2d.h"

USING_NS_CC;

const int ButtonWithBatchedSprite::kTouchPriority = 1;

ButtonWithBatchedSprite* ButtonWithBatchedSprite::createWithSpriteFrameName(const std::string& spriteFrameName)
{
    ButtonWithBatchedSprite* button = new (std::nothrow) ButtonWithBatchedSprite();
    if (button && button->initWithSpriteFrameName(spriteFrameName))
    {
        button->autorelease();
        return button;
    }
    delete button;
    return nullptr;
}

ButtonWithBatchedSprite::ButtonWithBatchedSprite()
{
}

// @ios 1000298c8 (dealloc)
ButtonWithBatchedSprite::~ButtonWithBatchedSprite()
{
    CC_SAFE_RELEASE(_disabledSprite);
    CC_SAFE_RELEASE(_pressSprite);
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
        _touchListener = nullptr;
    }
}

// @ios 1000297a4
bool ButtonWithBatchedSprite::initWithSpriteFrame(SpriteFrame* spriteFrame)
{
    if (!Sprite::initWithSpriteFrame(spriteFrame))
    {
        return false;
    }
    const Size& size = getContentSize();
    _bounds = cg::Rect(0.0, 0.0, size.width, size.height);
    center = cg::Point(size.width * 0.5, size.height * 0.5);
    _isPressed = false;
    _isEnabled = true;
    _isHoldable = false;
    _swallowsTouch = true;
    _isToggleable = false;
    _hideDefaultOnPress = true;
    defaultSprite = this;
    return true;
}

// @ios 100029948
bool ButtonWithBatchedSprite::isPressed()
{
    return _isPressed;
}

// @ios 100029978
void ButtonWithBatchedSprite::setIsEnabled(bool isEnabled)
{
    if (_isPressed && _isHoldable)
    {
        _isPressed = false;
        showDefaultState();
    }
    _isEnabled = isEnabled;
}

// @ios 1000299d0
void ButtonWithBatchedSprite::setBounds(const cg::Rect& bounds)
{
    _bounds = bounds;
}

// @ios 1000299e8
void ButtonWithBatchedSprite::onEnterTransitionDidFinish()
{
    // [[CCDirector sharedDirector].touchDispatcher addTargetedDelegate:self priority:0
    //                                                  swallowsTouches:_swallowsTouch]
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
    }
    _touchListener = EventListenerTouchOneByOne::create();
    _touchListener->setSwallowTouches(_swallowsTouch);
    _touchListener->onTouchBegan = [this](Touch* t, Event* e) { return ccTouchBegan(t, e); };
    _touchListener->onTouchMoved = [this](Touch* t, Event* e) { ccTouchMoved(t, e); };
    _touchListener->onTouchEnded = [this](Touch* t, Event* e) { ccTouchEnded(t, e); };
    _touchListener->onTouchCancelled = [this](Touch* t, Event* e) { ccTouchCancelled(t, e); };
    _eventDispatcher->addEventListenerWithFixedPriority(_touchListener, kTouchPriority);
    Sprite::onEnterTransitionDidFinish();
}

// @ios 100029a50
void ButtonWithBatchedSprite::onExit()
{
    removeStateSprites();
    unscheduleAllCallbacks();
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
        _touchListener = nullptr;
    }
    Sprite::onExit();
}

// @ios 100029ab4
void ButtonWithBatchedSprite::setToUpState(float dt)
{
    unschedule(CC_SCHEDULE_SELECTOR(ButtonWithBatchedSprite::setToUpState));
    _isPressed = false;
    showDefaultState();
}

// @ios 100029aec
bool ButtonWithBatchedSprite::ccTouchBegan(Touch* touch, Event* event)
{
    if (!_isEnabled)
    {
        return false;
    }
    cg::Point location(convertToNodeSpace(touch->getLocation()));
    if (!_isToggleable)
    {
        if (_isPressed)
        {
            return false;
        }
        if (!cg::rectContainsPoint(_bounds, location))
        {
            return false;
        }
        if (onPressInvocation)
        {
            onPressInvocation(this);
        }
        _isPressed = true;
        showPressedState();
        if (_isHoldable || _isToggleable)
        {
            return true;
        }
        schedule(CC_SCHEDULE_SELECTOR(ButtonWithBatchedSprite::setToUpState), _rateLimit);
        return true;
    }
    if (!cg::rectContainsPoint(_bounds, location))
    {
        return false;
    }
    if (_isPressed)
    {
        _isPressed = false;
        showDefaultState();
        return true;
    }
    _isPressed = true;
    showPressedState();
    return true;
}

// @ios 100029cc4
void ButtonWithBatchedSprite::ccTouchMoved(Touch* touch, Event* event)
{
    if (!_isEnabled || !_isPressed || !_isHoldable || _isToggleable)
    {
        return;
    }
    cg::Point location(convertToNodeSpace(touch->getLocation()));
    if (!cg::rectContainsPoint(_bounds, location))
    {
        _isPressed = false;
        if (onRollOffInvocation)
        {
            onRollOffInvocation(this);
        }
        showDefaultState();
    }
}

// @ios 100029db0
void ButtonWithBatchedSprite::ccTouchEnded(Touch* touch, Event* event)
{
    if (!_isEnabled || !_isPressed || _isToggleable)
    {
        return;
    }
    if (_isHoldable)
    {
        _isPressed = false;
        showDefaultState();
    }
    if (onReleaseInvocation)
    {
        invokeOnReleaseMethod();
    }
}

// @ios 100029e38
void ButtonWithBatchedSprite::invokeOnReleaseMethod()
{
    RefPtr<ButtonWithBatchedSprite> keep(this);
    Callback callback = onReleaseInvocation;
    callback(this);
}

// @ios 100029e48
void ButtonWithBatchedSprite::ccTouchCancelled(Touch* touch, Event* event)
{
    if (_isEnabled && _isPressed && !_isToggleable && _isHoldable)
    {
        _isPressed = false;
        showDefaultState();
    }
}

// @ios 100029e94
void ButtonWithBatchedSprite::showDefaultState()
{
    defaultSprite->setVisible(true);
    if (_pressSprite)
    {
        _pressSprite->setVisible(false);
    }
    if (_disabledSprite)
    {
        _disabledSprite->setVisible(false);
    }
}

// @ios 100029efc
void ButtonWithBatchedSprite::showPressedState()
{
    if (_pressSprite)
    {
        if (_hideDefaultOnPress)
        {
            defaultSprite->setVisible(false);
        }
        _pressSprite->setVisible(true);
    }
    if (_disabledSprite)
    {
        _disabledSprite->setVisible(false);
    }
}

// @ios 100029f78
void ButtonWithBatchedSprite::showDisabledState()
{
    if (_pressSprite)
    {
        _pressSprite->setVisible(false);
    }
    if (_disabledSprite)
    {
        defaultSprite->setVisible(false);
        _disabledSprite->setVisible(true);
    }
}

// @ios 100029fe4
void ButtonWithBatchedSprite::setDisabledSprite(Sprite* sprite)
{
    Node* parent = getParent();
    // (sic) the iOS code tests pressSprite before removing the old disabled sprite.
    if (_pressSprite && _disabledSprite && parent)
    {
        parent->removeChild(_disabledSprite, false);
    }
    if (parent)
    {
        parent->addChild(sprite);
    }
    CC_SAFE_RETAIN(sprite);
    CC_SAFE_RELEASE(_disabledSprite);
    _disabledSprite = sprite;
    _disabledSprite->setPosition(getPosition());
    _disabledSprite->setRotation(getRotation());
    _disabledSprite->setFlippedX(isFlippedX());
    _disabledSprite->setFlippedY(isFlippedY());
    _disabledSprite->setVisible(false);
}

// @ios 10002a0a0
void ButtonWithBatchedSprite::setPressSprite(Sprite* sprite)
{
    Node* parent = getParent();
    if (_pressSprite && parent)
    {
        parent->removeChild(_pressSprite, false);
    }
    if (parent)
    {
        parent->addChild(sprite);
    }
    CC_SAFE_RETAIN(sprite);
    CC_SAFE_RELEASE(_pressSprite);
    _pressSprite = sprite;
    _pressSprite->setPosition(getPosition());
    _pressSprite->setRotation(getRotation());
    _pressSprite->setFlippedX(isFlippedX());
    _pressSprite->setFlippedY(isFlippedY());
    _pressSprite->setVisible(false);
}

// @ios 10002a14c
void ButtonWithBatchedSprite::setIsHoldable(bool isHoldable)
{
    _isHoldable = isHoldable;
}

// @ios 10002a15c
void ButtonWithBatchedSprite::setOnPressTarget(Callback callback)
{
    onPressInvocation = std::move(callback);
}

// @ios 10002a1ec
void ButtonWithBatchedSprite::setOnRollOffTarget(Callback callback)
{
    onRollOffInvocation = std::move(callback);
}

// @ios 10002a27c
void ButtonWithBatchedSprite::setOnReleaseTarget(Callback callback)
{
    onReleaseInvocation = std::move(callback);
    // A release target makes the button holdable (iOS sets isHoldable here).
    _isHoldable = true;
}

// @ios 10002a320
// port: cocos' Node::setPosition(const Vec2&) forwards to the (x, y) overload, which does the work,
// so the iOS body lives there and this one forwards (overriding both ways round would recurse).
void ButtonWithBatchedSprite::setPosition(const Vec2& position)
{
    setPosition(position.x, position.y);
}

void ButtonWithBatchedSprite::setPosition(float x, float y)
{
    Sprite::setPosition(x, y);
    if (_pressSprite)
    {
        _pressSprite->setPosition(x, y);
    }
    if (_disabledSprite)
    {
        _disabledSprite->setPosition(x, y);
    }
}

// @ios 10002a3a0
void ButtonWithBatchedSprite::removeStateSprites()
{
}

// @ios 10002a3a4
void ButtonWithBatchedSprite::die()
{
    removeStateSprites();
    unscheduleAllCallbacks();
    if (_touchListener)
    {
        _eventDispatcher->removeEventListener(_touchListener);
        _touchListener = nullptr;
    }
}

// @ios 10002a3e0
void ButtonWithBatchedSprite::alignSprites()
{
    if (_pressSprite)
    {
        _pressSprite->setPosition(defaultSprite->getPosition());
        _pressSprite->setRotation(defaultSprite->getRotation());
        _pressSprite->setFlippedX(isFlippedX());
        _pressSprite->setFlippedY(isFlippedY());
    }
    if (_disabledSprite)
    {
        _disabledSprite->setPosition(defaultSprite->getPosition());
        _disabledSprite->setRotation(defaultSprite->getRotation());
        _disabledSprite->setFlippedX(isFlippedX());
        _disabledSprite->setFlippedY(isFlippedY());
    }
}

// @ios 10002a4d8
void ButtonWithBatchedSprite::padHitSpace(float padding)
{
    _bounds.origin.x = (double)(padding * -0.5f);
    _bounds.origin.y = (double)(padding * -0.5f);
    _bounds.size.height = _bounds.size.height + (double)padding;
    _bounds.size.width = _bounds.size.width + (double)padding;
}

// @ios 10002a510
void ButtonWithBatchedSprite::setIsPressed(bool isPressed)
{
    _isPressed = isPressed;
}

// @ios 10002a520
bool ButtonWithBatchedSprite::isHoldable()
{
    return _isHoldable;
}

// @ios 10002a530
bool ButtonWithBatchedSprite::isToggleable()
{
    return _isToggleable;
}

// @ios 10002a540
void ButtonWithBatchedSprite::setIsToggleable(bool isToggleable)
{
    _isToggleable = isToggleable;
}

// @ios 10002a550
float ButtonWithBatchedSprite::rateLimit()
{
    return _rateLimit;
}

// @ios 10002a560
void ButtonWithBatchedSprite::setRateLimit(float rateLimit)
{
    _rateLimit = rateLimit;
}

// @ios 10002a570
Sprite* ButtonWithBatchedSprite::disabledSprite()
{
    return _disabledSprite;
}

// @ios 10002a580
Sprite* ButtonWithBatchedSprite::pressSprite()
{
    return _pressSprite;
}

// @ios 10002a590
bool ButtonWithBatchedSprite::isEnabled()
{
    return _isEnabled;
}

// @ios 10002a5a0
cg::Rect ButtonWithBatchedSprite::bounds()
{
    return _bounds;
}

// @ios 10002a5b8
const Value& ButtonWithBatchedSprite::userObject()
{
    return _userObject;
}

// @ios 10002a5c8
void ButtonWithBatchedSprite::setUserObject(const Value& userObject)
{
    _userObject = userObject;
}

// @ios 10002a5d4
bool ButtonWithBatchedSprite::swallowsTouch()
{
    return _swallowsTouch;
}

// @ios 10002a5e4
void ButtonWithBatchedSprite::setSwallowsTouch(bool swallowsTouch)
{
    _swallowsTouch = swallowsTouch;
}

// @ios 10002a5f4
bool ButtonWithBatchedSprite::hideDefaultOnPress()
{
    return _hideDefaultOnPress;
}

// @ios 10002a608
void ButtonWithBatchedSprite::setHideDefaultOnPress(bool hideDefaultOnPress)
{
    _hideDefaultOnPress = hideDefaultOnPress;
}
