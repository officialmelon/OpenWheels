#pragma once
// Rows of the QoL pages built on the original OptionsMenuItem (same font, background and
// pressed fade), so they look like the game's own Options rows.

#include <functional>
#include <string>

#include "OptionsMenuItem.h"

// A volume-style slider: the row fills from the left up to its value (0..1) in the game's blue;
// click or drag along the row to set it (5 % steps). `label` gives the text for a value.
class QoLSliderItem : public OptionsMenuItem
{
public:
    static QoLSliderItem* create(std::function<std::string(float)> label, std::function<float()> get,
                                 std::function<void(float)> set);
    void refresh();
    // PAD (PC addition): one 5 % step left (-1) or right (+1), for controller focus navigation.
    void nudge(int direction);

private:
    bool initSlider(std::function<std::string(float)> label, std::function<float()> get,
                    std::function<void(float)> set);
    void setFromTouch(cocos2d::Touch* touch);
    void drawFill(float value);

    std::function<std::string(float)> _label;
    std::function<float()> _get;
    std::function<void(float)> _set;
    cocos2d::DrawNode* _fill = nullptr;
};

// An OptionsMenuItem of another width (the original rows are 1500 wide).
class QoLRowItem : public OptionsMenuItem
{
public:
    static QoLRowItem* create(const std::string& text, int tag, float width,
                              const std::function<void(cocos2d::Ref*)>& callback,
                              OptionsMenuItemAppearance appearance);
    void setTextColor(const cocos2d::Color3B& color);
};
