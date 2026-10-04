#include "qol/QoLWidgets.h"

#include <algorithm>
#include <cmath>

#include "Globals.h"

USING_NS_CC;

QoLSliderItem* QoLSliderItem::create(std::function<std::string(float)> label, std::function<float()> get,
                                     std::function<void(float)> set)
{
    QoLSliderItem* item = new (std::nothrow) QoLSliderItem();
    if (item && item->initSlider(std::move(label), std::move(get), std::move(set))) {
        item->autorelease();
        return item;
    }
    delete item;
    return nullptr;
}

bool QoLSliderItem::initSlider(std::function<std::string(float)> label, std::function<float()> get,
                               std::function<void(float)> set)
{
    _label = std::move(label);
    _get = std::move(get);
    _set = std::move(set);
    if (!OptionsMenuItem::init(_label(_get()), 0, nullptr, OptionsMenuItemAppearanceDefault)) {
        return false;
    }
    // The fill sits between the row's background and its text.
    _fill = DrawNode::create();
    _container->addChild(_fill, 0);
    _textLabel->setLocalZOrder(1);
    drawFill(_get());

    // Own touch handling (the Menu only reports taps): the item's listener runs before the
    // Menu's (scene graph priority, child above parent) and swallows touches on the row.
    auto listener = EventListenerTouchOneByOne::create();
    listener->setSwallowTouches(true);
    listener->onTouchBegan = [this](Touch* touch, Event*) {
        if (!isEnabled() || !isVisible()) return false;
        for (Node* n = this; n; n = n->getParent()) {
            if (!n->isVisible()) return false;
        }
        const Vec2 local = convertToNodeSpace(touch->getLocation());
        const Size size = getContentSize();
        if (local.x < 0 || local.y < 0 || local.x > size.width || local.y > size.height) return false;
        selected();
        setFromTouch(touch);
        return true;
    };
    listener->onTouchMoved = [this](Touch* touch, Event*) { setFromTouch(touch); };
    listener->onTouchEnded = [this](Touch* touch, Event*) {
        setFromTouch(touch);
        unselected();
        UserDefault::getInstance()->flush();
    };
    listener->onTouchCancelled = [this](Touch*, Event*) { unselected(); };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
    return true;
}

void QoLSliderItem::setFromTouch(Touch* touch)
{
    const Vec2 local = convertToNodeSpace(touch->getLocation());
    float value = local.x / getContentSize().width;
    value = std::max(0.0f, std::min(1.0f, std::round(value * 20.0f) / 20.0f));
    if (std::fabs(value - _get()) < 0.001f) return;
    _set(value);
    refresh();
}

void QoLSliderItem::refresh()
{
    const float value = _get();
    setLabelText(_label(value));
    drawFill(value);
}

void QoLSliderItem::drawFill(float value)
{
    _fill->clear();
    if (value <= 0.0f) return;
    const Size size = getContentSize();
    const Color3B& blue = globals::colors::blue;
    _fill->drawSolidRect(Vec2::ZERO, Vec2(size.width * value, size.height),
                         Color4F(blue.r / 255.0f, blue.g / 255.0f, blue.b / 255.0f, 0.45f));
}

QoLRowItem* QoLRowItem::create(const std::string& text, int tag, float width,
                               const std::function<void(Ref*)>& callback, OptionsMenuItemAppearance appearance)
{
    QoLRowItem* item = new (std::nothrow) QoLRowItem();
    item->init(text, tag, callback, appearance);
    // Same row at another width: redraw the background and re-centre the text.
    const Size size(width, item->getContentSize().height);
    Color4F colour = appearance == OptionsMenuItemAppearanceRed ? Color4F::RED : Color4F::WHITE;
    item->_background->clear();
    item->_background->drawSolidRect(Vec2::ZERO, Vec2(size.width, size.height), colour);
    item->_textLabel->setPosition(Vec2(size.width / 2, size.height / 2 - 10));
    item->setContentSize(size);
    item->autorelease();
    return item;
}

void QoLRowItem::setTextColor(const Color3B& color)
{
    _textLabel->setTextColor(Color4B(color));
}
