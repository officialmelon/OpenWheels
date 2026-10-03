#pragma once

#include "Special.h"

class InputObject;

// iOS BottleRef : Special (instanceSize 0x268) - editor reference for the bottle, level item 20
// (-> Bottle).
//
// Sprite: "e_1x1.png"; setBottleType replaces all children with "e_bottle_<type>.png" and sets
// refRect to its centred content size. canDragModify 0, canRotate 1. No shape count.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 angle, p3 bottleType, p4 sleeping,
// p5 interactive (Bottle::init: p3 colorId int, p4/p5 bools).
// Defaults: bottleType 1. UI keys = propertyKeys (xMeters/yMeters get no input - iOS behaviour).
class BottleRef : public Special
{
public:
    CREATE_FUNC(BottleRef);

    bool init() override;                                                   // @ios 100089064
    // bottleType: SliderInputObject "BOTTLE TYPE", 1..4, 3 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 100089198
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000892bc
    // Clamped to 1..4.
    void setBottleType(unsigned int bottleType);                            // @ios 100089364
    unsigned int bottleType();                                              // @ios 100089414

    // port: KVC (bottleType), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    unsigned int _bottleType = 0;     // +0x264
};
