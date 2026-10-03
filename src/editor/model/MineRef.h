#pragma once

#include "Special.h"

class InputObject;

// iOS MineRef : Special (instanceSize 0x268) - editor reference for the mine, level item 2
// (-> Mine).
//
// Sprite: "e_mine.png"; the create hook sets refRect to (0, 0, texture w, h). Shape count 2.
// propertyKeys: Special's defaults + slowMoDuration -> p0 xMeters, p1 yMeters, p2 angle,
// p3 fixed, p4 sleeping, p5 slowMoDuration (Mine::init reads p0..p2 only: the iOS slow-motion
// duration has no effect in the Android game).
// Defaults: slowMoDuration 0.
// UI keys: x, y, angle, fixed, [sleeping only when !fixed], slowMoDuration.
class MineRef : public Special
{
public:
    CREATE_FUNC(MineRef);

    bool init() override;                                                   // @ios 1000d5f00
    void onEnter() override;                                                // @ios 1000d5f88
    // Only when the value changes: posts ref_ui_keys_will_change, Special::setFixed,
    // posts ref_ui_keys_changed.
    void setFixed(const cocos2d::Value& fixed) override;                    // @ios 1000d5fdc
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000d6080
    // slowMoDuration: SliderInputObject "SLOW DURACTION" (sic), initial value always 0,
    // 0..10, 0 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000d6108
    cocos2d::Value slowMoDuration();                                        // @ios 1000d6214
    void setSlowMoDuration(const cocos2d::Value& slowMoDuration);           // @ios 1000d6230
    void createRef() override;  // iOS -create                                 @ios 1000d6260

    // port: KVC (slowMoDuration, fixed), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    float _slowMoDuration = 0.0f;   // +0x264  iOS ivar "slowMoDuration" (clashes with the getter)
};
