#include "EditorLayerButton.h"

#include "cocos2d.h"

USING_NS_CC;

EditorLayerButton* EditorLayerButton::createWithSpriteFrameName(const std::string& spriteFrameName,
                                                                const std::string& icon)
{
    EditorLayerButton* button = new (std::nothrow) EditorLayerButton();
    if (button && button->initWithSpriteFrameName(spriteFrameName, icon))
    {
        button->autorelease();
        return button;
    }
    delete button;
    return nullptr;
}

EditorLayerButton* EditorLayerButton::createWithIcon(const std::string& icon)
{
    EditorLayerButton* button = new (std::nothrow) EditorLayerButton();
    if (button && button->initWithIcon(icon))
    {
        button->autorelease();
        return button;
    }
    delete button;
    return nullptr;
}

// @ios 1000dcca8
bool EditorLayerButton::initWithSpriteFrameName(const std::string& spriteFrameName, const std::string& icon)
{
    // [super initWithSpriteFrameName:] -> ButtonWithBatchedSprite::initWithSpriteFrame.
    if (!Sprite::initWithSpriteFrameName(spriteFrameName))
    {
        return false;
    }
    _iconSpriteFrameName = icon;
    _icon = Sprite::createWithSpriteFrameName(icon);
    const Size& size = getContentSize();
    _icon->setPosition(size.width * 0.5f, size.height * 0.5f);
    addChild(_icon, 1);
    return true;
}

// @ios 1000dcd5c
bool EditorLayerButton::initWithIcon(const std::string& icon)
{
    return initWithSpriteFrameName("editorui_btn.png", icon);
}

// @ios 1000dcd6c
void EditorLayerButton::setIsEnabled(bool isEnabled)
{
    ButtonWithBatchedSprite::setIsEnabled(isEnabled);
    if (isEnabled)
    {
        setOpacity(0xff);
        _icon->setOpacity(0xff);
        return;
    }
    _icon->setOpacity(0x59);
    setOpacity(0x3f);
}

// @ios 1000dce04
void EditorLayerButton::createPressState()
{
    createPressStateWithGrey(true);
}

// @ios 1000dce0c
void EditorLayerButton::createPressStateWithGrey(bool grey)
{
    Sprite* pressSprite =
        Sprite::createWithSpriteFrameName(grey ? "editorui_darkGreybtn.png" : "editorui_darkBluebtn.png");
    Sprite* icon = Sprite::createWithSpriteFrameName(_iconSpriteFrameName);
    const Size& size = getContentSize();
    icon->setPosition(Vec2(size.width * 0.5f, size.height * 0.5f));
    pressSprite->addChild(icon, 1);
    // port: the press sprite is a sibling in the batch node and needs the button's
    // atlas-to-point scale (iOS content scale handles this implicitly).
    pressSprite->setScale(getScaleX(), getScaleY());
    setPressSprite(pressSprite);
}

// @ios 1000dceb8
Sprite* EditorLayerButton::icon()
{
    return _icon;
}

// @ios 1000dcec8
void EditorLayerButton::setIcon(Sprite* icon)
{
    _icon = icon;
}
