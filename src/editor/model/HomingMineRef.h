#pragma once

#include "Special.h"

class InputObject;

// iOS HomingMineRef : Special (instanceSize 0x26c) - editor reference for the homing mine,
// level item 25 (-> HomingMine).
//
// Sprite: "e_homingMine.png". canRotate 1 but setRotation is a no-op. No shape count.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 speed, p3 delay
// (HomingMine::init: p2 seek speed (x 0.01), p3 counter).
// Defaults: speed 1, delay 0.
// UI keys: x, y, speed, delay.
class HomingMineRef : public Special
{
public:
    CREATE_FUNC(HomingMineRef);

    bool init() override;                                                   // @ios 1000dc934
    void setRotation(float rotation) override;  // no-op                       @ios 1000dca38
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000dca3c
    void createRef() override;  // iOS -create: empty                          @ios 1000dcac4
    // speed: SliderInputObject "SPEED", 1..10, 9 segments;
    // delay: SliderInputObject "DELAY", 0..5, 5 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000dcac8
    unsigned int speed();                                                   // @ios 1000dcc68
    void setSpeed(unsigned int speed);                                      // @ios 1000dcc78
    unsigned int delay();                                                   // @ios 1000dcc88
    void setDelay(unsigned int delay);                                      // @ios 1000dcc98

    // port: KVC (speed, delay), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    unsigned int _speed = 0;   // +0x264
    unsigned int _delay = 0;   // +0x268
};
