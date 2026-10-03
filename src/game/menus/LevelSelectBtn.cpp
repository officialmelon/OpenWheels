#include "LevelSelectBtn.h"

#include "Patch.h"

USING_NS_CC;

// @005df840
LevelSelectBtn::LevelSelectBtn()
    : _bg(nullptr)
    , _chapter(-1)
    , _level(-1)
    , _locked(false)
    , _isLevelSlot(true)
    , _touch(nullptr)
{
}

// @005df888 (D1), @005df88c (D0)
LevelSelectBtn::~LevelSelectBtn()
{
}

// @005df8b0
LevelSelectBtn* LevelSelectBtn::create(int chapter, int level, bool locked, bool completed)
{
    LevelSelectBtn* btn = new LevelSelectBtn();
    btn->init(chapter, level, locked, completed);
    btn->autorelease();
    return btn;
}

// @005df930
bool LevelSelectBtn::init(int chapter, int level, bool locked, bool completed)
{
    _chapter = chapter;
    _level = level;
    _locked = locked;

    if (chapter == -1 || level == -1)
    {
        // Empty slot of the 5x3 grid.
        _isLevelSlot = false;
        _bg = Sprite::createWithSpriteFrameName("levSelect_outline.png");
        _bg->setOpacity(63);
        addChild(_bg);
    }
    else
    {
        _bg = Sprite::createWithSpriteFrameName("levSelect_icon.png");
        _bg->setOpacity(63);
        addChild(_bg);

        if (_locked)
        {
            addChild(Sprite::createWithSpriteFrameName("levSelect_lock.png"));
        }
        else
        {
            int number = _level + 1;
            addChild(Sprite::createWithSpriteFrameName("levSelect_" + patch::to_string(number) + ".png"));
        }

        if (completed)
        {
            Sprite* check = Sprite::createWithSpriteFrameName("levSelect_check.png");
            check->setTag(1);
            // This node's own content size (never set, so the check mark sits at the centre).
            Size size = getContentSize();
            check->setPosition(Vec2(size.width * 0.5f, size.height * 0.5f));
            addChild(check);
        }
    }
    return true;
}

// @005dfcc4
Size LevelSelectBtn::getSize()
{
    return _bg->getTextureRect().size;
}

// @005dfcd4
void LevelSelectBtn::setTouch(Touch* touch)
{
    if (_isLevelSlot)
    {
        _bg->setOpacity(touch != nullptr ? 255 : 63);
    }
    _touch = touch;
}

// @005dfd1c
void LevelSelectBtn::showPressedState(bool pressed)
{
    _bg->setOpacity(pressed ? 255 : 63);
}

// @005dfd38
Touch* LevelSelectBtn::getTouch()
{
    return _touch;
}

// @005dfd40
Rect LevelSelectBtn::getHitArea()
{
    Rect hitArea;
    Size size = _bg->getContentSize();
    const Vec2& position = getPosition();
    hitArea.origin = Vec2(position.x + size.width * -0.5f, position.y + size.height * -0.5f);
    hitArea.size = size;
    return hitArea;
}

// @005dfde8
bool LevelSelectBtn::getLocked()
{
    return _locked;
}

// @005dfdf0
int LevelSelectBtn::getChapter()
{
    return _chapter;
}

// @005dfdf8
int LevelSelectBtn::getLevel()
{
    return _level;
}

// @005dfe00
void LevelSelectBtn::setChapter(int chapter)
{
    _chapter = chapter;
}

// @005dfe08
void LevelSelectBtn::setLevel(int level)
{
    _level = level;
}
