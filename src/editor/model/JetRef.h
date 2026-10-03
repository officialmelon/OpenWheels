#pragma once

#include "Special.h"

class InputObject;

// iOS JetRef : Special (instanceSize 0x27d) - editor reference for the jet, level item 28
// (-> Jet).
//
// Sprite: "e_1x1.png" with child _mc = "e_jet.png" at (0, ptm * -0.096); updateSprite scales
// the ref itself (not _mc) to 0.5 + (power - 1) * 0.0555556. Shape count 1 (set in onEnter).
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 angle, p3 sleeping, p4 power,
// p5 firingTime, p6 accelTime, p7 fixedAngle (Jet::init: p3 bool, p4 float, p5/p6 int, p7 bool).
// Defaults: power 1, firingTime 0, accelTime 0, fixedAngle 0.
// UI keys: x, y, angle, sleeping, power, firingTime, accelTime, fixedAngle.
class JetRef : public Special
{
public:
    CREATE_FUNC(JetRef);

    bool init() override;                                                   // @ios 10008c020
    cocos2d::Value power();                    // @(int)                       @ios 10008c1b0
    void setPower(const cocos2d::Value& power); // then updateSprite           @ios 10008c1cc
    void updateSprite();                                                    // @ios 10008c200
    cocos2d::Value firingTime();                                            // @ios 10008c294
    void setFiringTime(const cocos2d::Value& firingTime);                   // @ios 10008c2b0
    cocos2d::Value accelTime();                                             // @ios 10008c2e0
    void setAccelTime(const cocos2d::Value& accelTime);                     // @ios 10008c2fc
    cocos2d::Value fixedAngle();               // @(bool)                      @ios 10008c32c
    void setFixedAngle(const cocos2d::Value& fixedAngle);                   // @ios 10008c348
    void onEnter() override;                                                // @ios 10008c378
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 10008c3c4
    // power: SliderInputObject "POWER", 1..10, 9 segments; firingTime: "FIRING TIME", 0..50,
    // 50 segments; accelTime: "ACCEL TIME", 0..5, 5 segments; fixedAngle: SwitchInputObject
    // "FIXED ANGLE"; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 10008c47c

    // port: KVC (power, firingTime, accelTime, fixedAngle), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    cocos2d::Sprite* _mc = nullptr;   // +0x268
    unsigned int _power = 0;          // +0x270
    unsigned int _firingTime = 0;     // +0x274
    unsigned int _accelTime = 0;      // +0x278
    bool _fixedAngle = false;         // +0x27c
};
