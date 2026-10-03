#include "OptionsMenuItem.h"

#include "Globals.h"

USING_NS_CC;

// @005fcec8
OptionsMenuItem* OptionsMenuItem::create(std::string text, int tag, const std::function<void(Ref*)>& callback,
                                         OptionsMenuItemAppearance appearance)
{
    // No null check and no check of init()'s result (as in the original).
    OptionsMenuItem* item = new (std::nothrow) OptionsMenuItem();
    item->init(text, tag, callback, appearance);
    item->autorelease();
    return item;
}

// @005fd060
bool OptionsMenuItem::init(std::string text, int tag, const std::function<void(Ref*)>& callback,
                           OptionsMenuItemAppearance appearance)
{
    _appearance = appearance;
    _originalColor = getColor();
    setTag(tag);

    _textLabel = Label::createWithTTF(text, "fonts/ClarendonLTStd-Bold.ttf", 100.0f);

    Color4B textColor = Color4B::WHITE;
    Color4F backgroundColor = Color4F::WHITE;
    if (_appearance == OptionsMenuItemAppearanceBlue)
    {
        textColor = Color4B(globals::colors::blue, 255);
        backgroundColor = Color4F::WHITE;
    }
    else if (_appearance == OptionsMenuItemAppearanceRed)
    {
        textColor = Color4B::RED;
        backgroundColor = Color4F::RED;
    }

    _textLabel->setTextColor(textColor);
    _textLabel->setAlignment(TextHAlignment::CENTER);
    _textLabel->setAnchorPoint(Vec2(0.5f, 0.5f));

    Size size = _textLabel->getContentSize();
    size.height += 70.0f;
    size.width = 1500.0f;

    _container = Node::create();
    _background = DrawNode::create(2.0f);
    _background->drawSolidRect(Vec2(0.0f, 0.0f), Vec2(size.width, size.height), backgroundColor);
    _background->setOpacity(_backgroundOpacity);
    _container->addChild(_background);
    _container->addChild(_textLabel);
    _textLabel->setPosition(Vec2(size.width / 2, size.height / 2 - 10));

    bool result = MenuItemLabel::initWithLabel(_container, callback);
    setContentSize(size);
    return result;
}

// @005fd340
void OptionsMenuItem::setLabelText(std::string text)
{
    _textLabel->setString(text);
}

// @005fd350
void OptionsMenuItem::selected()
{
    if (!_enabled)
    {
        return;
    }

    _textLabel->stopActionByTag(_labelActionTag);
    _background->stopActionByTag(_backgroundActionTag);
    MenuItem::selected();

    if (_appearance == OptionsMenuItemAppearanceDefault)
    {
        TintTo* tint = TintTo::create(_animationDuration, globals::colors::pink);
        tint->setTag(_labelActionTag);
        _textLabel->runAction(tint);
    }

    FadeTo* fade = FadeTo::create(_animationDuration, _backgroundSelectedOpacity);
    fade->setTag(_backgroundActionTag);
    _background->runAction(fade);
}

// @005fd3f8
void OptionsMenuItem::unselected()
{
    if (!_enabled)
    {
        return;
    }

    _textLabel->stopActionByTag(_labelActionTag);
    _background->stopActionByTag(_backgroundActionTag);
    // The original calls selected() here, not unselected() (kept as is).
    MenuItem::selected();

    if (_appearance == OptionsMenuItemAppearanceDefault)
    {
        // No tag is set on this tint (as in the original).
        _textLabel->runAction(TintTo::create(_animationDuration, _originalColor));
    }

    FadeTo* fade = FadeTo::create(_animationDuration, _backgroundOpacity);
    fade->setTag(_backgroundActionTag);
    _background->runAction(fade);
}

// @005fd494
void OptionsMenuItem::activate()
{
    if (!_enabled)
    {
        return;
    }

    _textLabel->stopAllActions();
    _textLabel->setColor(_originalColor);
    _background->stopAllActions();
    _background->setOpacity(_backgroundOpacity);
    MenuItem::activate();
}
