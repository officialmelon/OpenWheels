#pragma once

#include "Special.h"

class InputObject;

// iOS BoostPanelRef : Special (instanceSize 0x278) - editor reference for the boost panel strip,
// level item 12 (-> BoostPanel).
//
// Sprite: "e_1x1.png"; setUpSprites (also the create hook) lays out `num` panels, each a pair of
// "e_boostpanel.png" (second one scaleY -1), segWidth = ptm * 2.88 apart, and sets refRect.
// canDragModify. Shape count 1.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 angle, p3 num, p4 power
// (BoostPanel::init: p3 numPanels int, p4 power int).
// Defaults: num 2 (min 1, max 10), power 20.
// UI keys: x, y, angle, num, power.
class BoostPanelRef : public Special
{
public:
    CREATE_FUNC(BoostPanelRef);

    bool init() override;                                                   // @ios 1000c55a0
    void onEnter() override;                                                // @ios 1000c56e8
    void setNum(const cocos2d::Value& num);                                 // @ios 1000c573c
    unsigned int num();                                                     // @ios 1000c5770
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000c5780
    // num: SliderInputObject "PANELS", initial num, min/max from minNumPanels/maxNumPanels,
    //      segments = minNumPanels - maxNumPanels (negative -> huge unsigned; iOS bug, kept);
    // power: SliderInputObject "BOOST POWER", 10..100, 90 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000c5818
    void setUpSprites();                                                    // @ios 1000c59e0
    void createRef() override;  // iOS -create -> setUpSprites                 @ios 1000c5b3c
    unsigned int power();                                                   // @ios 1000c5b40
    void setPower(unsigned int power);                                      // @ios 1000c5b50

    // port: KVC (num, power), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    float segWidth = 0.0f;            // +0x264
    unsigned int minNumPanels = 0;    // +0x268
    unsigned int maxNumPanels = 0;    // +0x26c
    unsigned int _num = 0;            // +0x270  iOS ivar "num" (renamed: clashes with num())
    unsigned int _power = 0;          // +0x274
};
