#pragma once

#include "cocos2d.h"

#include <functional>
#include <string>

// Values from the binary. Enumerator names are invented (RE-TODO(@005fd060)).
enum OptionsMenuItemAppearance
{
    // White text; on select the text tints to globals::colors::pink.
    OptionsMenuItemAppearanceDefault = 0,
    // Text Color4B(globals::colors::blue, 255); no tint ("advanced options").
    OptionsMenuItemAppearanceBlue = 1,
    // Text Color4B::RED and background Color4F::RED; no tint ("reset level progress").
    OptionsMenuItemAppearanceRed = 2,
};

// Text row of the options/info menus: a 1500 x (text height + 70) DrawNode background (opacity 15,
// faded to 90 while pressed) behind a centred ClarendonLTStd-Bold 100 label, wrapped in a container
// Node that becomes the MenuItemLabel's label.
//
// arm64 sizeof 0x380 (MenuItemLabel nvsize 0x348). There is no user-provided ctor: create() uses
// `new (std::nothrow) OptionsMenuItem()`, which zero-fills, then runs the NSDMIs below. The
// destructor is implicit as well.
class OptionsMenuItem : public cocos2d::MenuItemLabel
{
public:
    // Does not check the result of init() and does not null-check before autorelease (as in the
    // original).
    static OptionsMenuItem* create(std::string text, int tag, const std::function<void(cocos2d::Ref*)>& callback,
                                   OptionsMenuItemAppearance appearance);         // @005fcec8
    bool init(std::string text, int tag, const std::function<void(cocos2d::Ref*)>& callback,
              OptionsMenuItemAppearance appearance);                              // @005fd060

    void setLabelText(std::string text);    // @005fd340  _textLabel->setString(text)

    void activate() override;               // @005fd494  vptr+0x528
    void selected() override;               // @005fd350  vptr+0x530
    // Calls MenuItem::selected() rather than MenuItem::unselected() (bug kept from the original).
    void unselected() override;             // @005fd3f8  vptr+0x538

    // ~OptionsMenuItem(): implicit. D0 @005fd500; the D1 slot is MenuItemLabel::~MenuItemLabel.

protected:
    OptionsMenuItemAppearance _appearance;  // +0x348
    cocos2d::Node* _container;              // +0x350  passed to MenuItemLabel::initWithLabel
    cocos2d::DrawNode* _background;         // +0x358
    cocos2d::Label* _textLabel;             // +0x360
    cocos2d::Color3B _originalColor;        // +0x368  = getColor() in init (WHITE)
    int _labelActionTag = 0;                // +0x36c  tag of the label's TintTo
    int _backgroundActionTag = 0;           // +0x370  tag of the background's FadeTo
    int _backgroundOpacity = 15;            // +0x374  normal background opacity
    int _backgroundSelectedOpacity = 90;    // +0x378  pressed background opacity
    float _animationDuration = 0.1f;        // +0x37c  TintTo/FadeTo duration
};
