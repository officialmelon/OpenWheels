#include "PageControl.h"

#include "PageControlDelegate.h"

#include <cmath>

USING_NS_CC;

// @005fd524
PageControl* PageControl::create(std::string spriteFrameName, Vec2 position, float spacing, int numPages,
                                 int page)
{
    PageControl* control = new (std::nothrow) PageControl();
    if (control != nullptr)
    {
        control->init(spriteFrameName, position, spacing, numPages, page);
        control->autorelease();
    }
    return control;
}

// @005fd648
bool PageControl::init(std::string spriteFrameName, Vec2 position, float spacing, int numPages, int page)
{
    float firstX = 0.0f;
    float lastX = 0.0f;
    for (int i = 0; i < numPages; i++)
    {
        Sprite* dot = Sprite::createWithSpriteFrameName(spriteFrameName);
        dot->setPosition(Vec2(position.x + (float)(numPages - 1) * spacing * -0.5f + spacing * (float)i,
                              position.y));
        if (i == 0)
        {
            firstX = dot->getPosition().x;
        }
        else if (i == numPages - 1)
        {
            lastX = dot->getPosition().x;
        }
        addChild(dot);
        _dots.push_back(dot);
    }

    _hitArea = Rect(firstX + spacing * -0.5f, position.y - spacing, (lastX - firstX) + spacing,
                    spacing + spacing);
    setPage(page);
    return true;
}

// @005fd9b4
PageControl::PageControl()
{
    cocos2d::log("PageControl: constructor");
    _touchListener = nullptr;
    _delegate = nullptr;
    _currentPage = -1;
}

// @005fda44 (D1), @005fdaf8 (D0)
PageControl::~PageControl()
{
    cocos2d::log("PageControl: destructor");
    removeListener();
}

// @005fdab8
void PageControl::removeListener()
{
    if (_touchListener != nullptr)
    {
        Director::getInstance()->getEventDispatcher()->removeEventListener(_touchListener);
        _touchListener->release();
        _touchListener = nullptr;
    }
}

// @005fdb1c
void PageControl::setPage(int page)
{
    if (_currentPage != (unsigned int)page)
    {
        _currentPage = page;
        for (size_t i = 0; i < _dots.size(); i++)
        {
            _dots[i]->setOpacity(i == _currentPage ? 255 : 64);
        }
    }
}

// @005fdb9c
void PageControl::setDelegate(PageControlDelegate* delegate)
{
    if (delegate != nullptr)
    {
        _delegate = delegate;
        addListener();
    }
    else
    {
        removeListener();
        _delegate = nullptr;
    }
}

// @005fdbf8
void PageControl::addListener()
{
    if (_touchListener != nullptr)
    {
        return;
    }

    _touchListener = EventListenerTouchOneByOne::create();
    _touchListener->retain();
    _touchListener->setSwallowTouches(true);
    // @005fe1a4 ($_0)
    _touchListener->onTouchBegan = [this](Touch* touch, Event* event) { return touchBegan(touch); };
    // @005fe284 ($_1)
    _touchListener->onTouchMoved = [this](Touch* touch, Event* event) { touchMoved(touch); };
    // @005fe30c ($_2)
    _touchListener->onTouchEnded = [this](Touch* touch, Event* event) { touchEnded(touch); };
    // @005fe394 ($_3)
    _touchListener->onTouchCancelled = [this](Touch* touch, Event* event) { touchCancelled(touch); };
    Director::getInstance()->getEventDispatcher()->addEventListenerWithSceneGraphPriority(_touchListener, this);
}

// @005fddc8
void PageControl::onExit()
{
    removeListener();
    Node::onExit();
}

// @005fde0c
bool PageControl::touchBegan(Touch* touch)
{
    return _hitArea.containsPoint(touch->getLocation());
}

// @005fde6c
void PageControl::touchMoved(Touch* touch)
{
    Vec2 location = touch->getLocation();
    if (_hitArea.containsPoint(location))
    {
        updateIndexWithTouchPosition(location);
    }
}

// @005fdf8c
void PageControl::updateIndexWithTouchPosition(Vec2 position)
{
    int numPages = (int)_dots.size();
    int index = floorf(((position.x - _hitArea.origin.x) / _hitArea.size.width) * (float)numPages);
    if (index < 0)
    {
        index = 0;
    }
    if (index >= numPages)
    {
        index = numPages - 1;
    }

    if (_currentPage != (unsigned int)index)
    {
        setPage(index);
        if (_delegate != nullptr)
        {
            _delegate->pageControl(this, _currentPage);
        }
    }
}

// @005fe068
void PageControl::touchEnded(Touch* touch)
{
    updateIndexWithTouchPosition(touch->getLocation());
}

// @005fe14c
void PageControl::touchCancelled(Touch* touch)
{
    touchEnded(touch);
}
