#pragma once

#include "Special.h"

class InputObject;

// iOS BladeWeaponRef : Special (instanceSize 0x29c) - editor reference for the blade weapon,
// level item 34 (-> BladeWeapon).
//
// Sprite: "e_1x1.png"; updateSprite replaces child tag 1 with "e_blade_<bladeType>.png" at
// (0.5, 0.5), scaleX -1 when reversed, and sets refRect to its centred texture rect.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 angle, p3 reverse, p4 sleeping,
// p5 interactive, p6 bladeType (BladeWeapon::init: p3 flipped, p4, p5 bools, p6 int).
// Defaults: bladeType 1, reverse 0. No shape count.
// UI keys: xMeters, yMeters, angle, reverse, sleeping, interactive, bladeType (xMeters/yMeters
// get no input from Special - iOS behaviour).
// The slider edits "bladeSliderIndex" (1..11) which maps through _bladeTypes
// {1,2,3,4,5,6,7,8,10,11,12}: type 9 is never offered.
class BladeWeaponRef : public Special
{
public:
    CREATE_FUNC(BladeWeaponRef);

    bool init() override;                                                   // @ios 1000f5814
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000f5970
    // bladeType: SliderInputObject "BLADE TYPE" on property bladeSliderIndex, 1..11, 10 segments;
    // reverse: SwitchInputObject "REVERSE"; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000f5a24
    // 1-based index of _bladeType in _bladeTypes (1 when not found).
    unsigned int bladeSliderIndex();                                        // @ios 1000f5bd4
    void setBladeSliderIndex(unsigned int index);                           // @ios 1000f5cc8
    // Values > 11 become 12 (no lower clamp).
    void setBladeType(unsigned int bladeType);                              // @ios 1000f5cec
    void updateSprite();                                                    // @ios 1000f5d08
    void setReverse(bool reverse);                                          // @ios 1000f5e10
    unsigned int bladeType();                                               // @ios 1000f5e20
    bool reverse();                                                         // @ios 1000f5e30

    // port: KVC (reverse, bladeType, bladeSliderIndex), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    unsigned int _bladeTypes[12] = {};   // +0x264  (11 used)
    bool _reverse = false;               // +0x294
    unsigned int _bladeType = 0;         // +0x298
};
