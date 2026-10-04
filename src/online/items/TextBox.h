#pragma once
// ONLINE (PC addition): browser special 16, TextBox (Flash userspecials/TextBox + editor
// TextBoxRef). A text field in the level's background layer (or a group's), with the browser
// game's embedded fonts (extracted from the SWF at build time), and the trigger actions
// "change opacity" (0) and "slide" (1).
//
// XML (TextBoxRef.getFullProperties): p0 x, p1 y (top-left of the text field), p2 angle,
// p3 colour, p4 font 1..5, p5 size 10..100, p6 align 1..3, p7 caption, p8 opacity 0..100.

#include "online/items/FlashSpecials.h"

#include "math/Vec2.h"

namespace cocos2d {
class Label;
class Node;
}

namespace online {

class TextBox : public FlashItem
{
public:
    ~TextBox() override;
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;

    bool isInstantAction(int action) override { return action != 0 && action != 1; }
    void prepareForTrigger() override;
    void triggerSingleActivation(LevelItem* trigger, int action, std::vector<float> properties) override;
    bool triggerRepeatActivation(LevelItem* trigger, int action, std::vector<float> properties,
                                 float time) override;
    void paintWithOffsetPoints(cocos2d::Vec2 offset, float rotation) override;
    void setOpacity(float opacity) override;

private:
    void applyAlpha();
    void placeAt(float xPx, float yPx);

    cocos2d::Node* _root = nullptr;   // positioned at the text field's top-left, rotated
    cocos2d::Label* _label = nullptr;
    bool _inGroup = false;
    float _alpha = 1.0f;              // textField.alpha
    bool _visible = true;             // textField.visible
    float _groupOpacity = 1.0f;
    float _x = 0.0f;                  // textField.x / y (Flash px; group-local in groups)
    float _y = 0.0f;
    // Running action state (Flash interpolates from the current value each frame).
    int _lastFrame[2] = {-1, -1};  // per action
};

}  // namespace online
