#include "GameplayBtn.h"

USING_NS_CC;

// @005b41bc
GameplayBtn::GameplayBtn()
    : _unpressedOpacity(102),
      _unpressedScale(0.86f)
{
    _stateValue = 0;
    _hitArea = Rect::ZERO;
    _touch = nullptr;
    _enabled = true;
    _adjustedScale = 1.0f;
}

// @005b424c (D1), @005b4258 (D0); thunks @005b4250, @005b427c
GameplayBtn::~GameplayBtn()
{
}

// @005b42a4
GameplayBtn* GameplayBtn::createWithSpriteFrameName(const std::string& spriteFrameName,
                                                    unsigned int stateValue, float userScale)
{
    GameplayBtn* btn = new GameplayBtn();
    if (!btn->initWithSpriteFrameName(spriteFrameName))
    {
        delete btn;
        return nullptr;
    }
    btn->autorelease();
    btn->_userScale = userScale;
    btn->setScale(btn->_unpressedScale * btn->_adjustedScale * userScale);
    btn->setOpacity(btn->_unpressedOpacity);
    btn->_stateValue = stateValue;
    return btn;
}

// @005b43fc
GameplayBtn* GameplayBtn::createWithSpriteFrameName(const std::string& spriteFrameName,
                                                    Vec2 position, Rect hitArea,
                                                    unsigned int stateValue, float userScale)
{
    GameplayBtn* btn = createWithSpriteFrameName(spriteFrameName, stateValue, userScale);
    if (btn)
    {
        btn->Sprite::setPosition(position);
        btn->setHitArea(Rect(Rect::ZERO));
        btn->setHitArea(hitArea);
    }
    return btn;
}

// @005b4374
void GameplayBtn::setUserScale(float userScale)
{
    _userScale = userScale;
}

// @005b437c
void GameplayBtn::showPressedState(bool pressed)
{
    if (pressed)
    {
        setScale(_adjustedScale * _userScale);
        setOpacity(255);
    }
    else
    {
        setScale(_unpressedScale * _adjustedScale * _userScale);
        setOpacity(_unpressedOpacity);
    }
    // QOL (PC addition): hidden touch controls stay invisible when pressed.
    if (_keyOnly)
    {
        setOpacity(0);
    }
}

// @005b43f4
void GameplayBtn::setStateValue(unsigned int value)
{
    _stateValue = value;
}

// @005b4564
void GameplayBtn::setPosition(Vec2 position)
{
    Sprite::setPosition(position);
    setHitArea(Rect(Rect::ZERO));
}

// @005b45d0
void GameplayBtn::setHitArea(Rect hitArea)
{
    if (hitArea.equals(Rect(0.0f, 0.0f, 0.0f, 0.0f)))
    {
        // the visible origin is fetched but not used
        Vec2 origin = Director::getInstance()->getVisibleOrigin();
        _hitArea = Rect(getPosition().x - getBoundingBox().size.width * 0.5f,
                        getPosition().y - getBoundingBox().size.height * 0.5f,
                        getBoundingBox().size.width, getBoundingBox().size.height);
    }
    else
    {
        _hitArea = hitArea;
    }
}

// @005b4728
void GameplayBtn::setTouch(Touch* touch)
{
    showPressedState(touch != nullptr);
    _touch = touch;
}

// @005b47ac
Touch* GameplayBtn::getTouch()
{
    return _touch;
}

// @005b47b4
Rect GameplayBtn::getHitArea()
{
    return _hitArea;
}

// @005b47c0
unsigned int GameplayBtn::getStateValue()
{
    return _stateValue;
}

// @005b47c8
void GameplayBtn::nudgeBounds(float top, float right, float bottom, float left)
{
    _hitArea.size.width = _hitArea.size.width + right + left;
    _hitArea.size.height = _hitArea.size.height + top + bottom;
    _hitArea.origin.x = _hitArea.origin.x - left;
    _hitArea.origin.y = _hitArea.origin.y - bottom;
}

// @005b47fc
void GameplayBtn::setEnabled(bool enabled)
{
    if (_enabled == enabled)
    {
        return;
    }
    _enabled = enabled;
    setOpacity(enabled && !_keyOnly ? _unpressedOpacity : 0);  // QOL (PC addition): && !_keyOnly
}

// @005b482c
bool GameplayBtn::getEnabled()
{
    return _enabled;
}

// @005b4834
void GameplayBtn::setAdjustedScale(float adjustedScale)
{
    _adjustedScale = adjustedScale;
    setScale(_unpressedScale * adjustedScale * _userScale);
}

// QOL (PC addition)
void GameplayBtn::setKeyOnly(bool keyOnly)
{
    _keyOnly = keyOnly;
    setOpacity(keyOnly || !_enabled ? 0 : _unpressedOpacity);
}

// QOL (PC addition)
bool GameplayBtn::getKeyOnly()
{
    return _keyOnly;
}
