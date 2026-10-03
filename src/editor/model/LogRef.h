#pragma once

#include "Special.h"

class InputObject;

// iOS LogRef : Special (instanceSize 0x278) - editor reference for the log, level item 4
// (-> Log).
//
// Sprite: "e_1x1.png" with child _logSprite = "e_log.png"; setUpSprites stretches it to
// _widthPoints x _heightPoints and sets refRect. canDragModify 0. Shape count 2.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 widthMeters, p3 heightMeters, p4 angle,
// p5 fixed, p6 sleeping, p7 strength (Log::init: p2 width, p3 height, p4 rotation, p5/p6 bools;
// p7 is not read on Android).
// Defaults: fixed 0, strength 5, width ptm * 0.576, height ptm * 6.4.
// UI keys: x, y, width, height, angle, fixed, [sleeping only when !fixed].
class LogRef : public Special
{
public:
    CREATE_FUNC(LogRef);

    bool init() override;                                                   // @ios 1000f4130
    void onEnter() override;                                                // @ios 1000f42d8
    void setStrength(const cocos2d::Value& strength);                       // @ios 1000f432c
    // Only when the value changes: posts ref_ui_keys_will_change, Special::setFixed,
    // posts ref_ui_keys_changed.
    void setFixed(const cocos2d::Value& fixed) override;                    // @ios 1000f435c
    cocos2d::Value strength();                                              // @ios 1000f4400
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000f441c
    // strength: SliderInputObject labelled with the literal "strength" (not localised),
    //           10..10, 9 segments - unreachable, strength is not a UI key;
    // width: SliderInputObject "WIDTH" (points), ptm * 0.576 .. ptm * 0.864, 0 segments;
    // height: SliderInputObject "HEIGHT", ptm * 3.2 .. ptm * 9.6, 0 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000f44ac
    void setWidth(float width) override;                                    // @ios 1000f46d8
    float width() override;                // iOS returns @(_widthPoints)      @ios 1000f46e8
    void setHeight(float height) override;                                  // @ios 1000f4704
    void setUpSprites();                                                    // @ios 1000f4714
    float height() override;               // iOS returns @(_heightPoints)     @ios 1000f47cc
    void setWidthMeters(const cocos2d::Value& widthMeters);                 // @ios 1000f47e8
    void setHeightMeters(const cocos2d::Value& heightMeters);               // @ios 1000f482c
    cocos2d::Value heightMeters();                                          // @ios 1000f4870
    cocos2d::Value widthMeters();                                           // @ios 1000f48a0
    void createRef() override;  // iOS -create                                 @ios 1000f48d0

    // port: KVC (width, height, widthMeters, heightMeters, strength, fixed), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    unsigned int _strength = 0;              // +0x264  iOS ivar "strength" (clashes with strength())
    float _widthPoints = 0.0f;               // +0x268
    float _heightPoints = 0.0f;              // +0x26c
    cocos2d::Sprite* _logSprite = nullptr;   // +0x270
};
