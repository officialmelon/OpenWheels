#pragma once

#include "Special.h"

class InputObject;

// iOS SpringBoxRef : Special (instanceSize 0x268) - editor reference for the spring box,
// level item 5 (-> SpringBox).
//
// Sprite: "e_springbox.png"; the create hook sets refRect to (0, 0, texture w, h).
// Shape count 2.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 angle, p3 delay (SpringBox::init: p3 float).
// Defaults: delay 0.
// UI keys: x, y, angle, delay.
class SpringBoxRef : public Special
{
public:
    CREATE_FUNC(SpringBoxRef);

    bool init() override;                                                   // @ios 1000b6024
    void onEnter() override;                                                // @ios 1000b6130
    cocos2d::Value delay();                    // @(float)                     @ios 1000b6184
    void setDelay(const cocos2d::Value& delay);                             // @ios 1000b61a0
    // delay: SliderInputObject "DELAY", 0..2, 4 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000b61d0
    void createRef() override;  // iOS -create                                 @ios 1000b62e4
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000b6328

    // port: KVC (delay), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    float _delay = 0.0f;   // +0x264  iOS ivar "delay" (clashes with delay())
};
