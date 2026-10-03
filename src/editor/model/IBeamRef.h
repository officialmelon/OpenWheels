#pragma once

#include "Special.h"

class InputObject;

// iOS IBeamRef : Special (instanceSize 0x26c) - editor reference for the I-beam, level item 3
// (-> IBeam).
//
// Sprite: "e_1x1.png"; setUpSprites (create hook) stretches "e_ibeam.png" to
// _widthPoints x _heightPoints and sets refRect. canDragModify. Shape count 1.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 widthMeters, p3 heightMeters, p4 angle,
// p5 fixed, p6 sleeping (IBeam::init: p2 width, p3 height, p4 rotation, p5/p6 bools;
// LevelB2D's group path also reads the rotation of t=3 from p4).
// Defaults: fixed 1, width ptm * 6.4 (6.4 m), height ptm * 0.512 (0.512 m).
// UI keys: x, y, width, height, angle, fixed, [sleeping only when !fixed].
// "width"/"height" are in points, "widthMeters"/"heightMeters" divide/multiply by ptmRatio.
class IBeamRef : public Special
{
public:
    CREATE_FUNC(IBeamRef);

    bool init() override;                                                   // @ios 1000ba794
    void onEnter() override;                                                // @ios 1000ba920
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000ba974
    // Only when the value changes: posts ref_ui_keys_will_change, Special::setFixed,
    // posts ref_ui_keys_changed.
    void setFixed(const cocos2d::Value& fixed) override;                    // @ios 1000baa10
    void setWidth(float width) override;   // points, then setUpSprites        @ios 1000baab4
    float width() override;                // iOS returns @(_widthPoints)      @ios 1000baac4
    void setHeight(float height) override; // points, then setUpSprites        @ios 1000baae0
    float height() override;               // iOS returns @(_heightPoints)     @ios 1000baaf0
    void setWidthMeters(const cocos2d::Value& widthMeters);   // no redraw     @ios 1000bab0c
    void setHeightMeters(const cocos2d::Value& heightMeters); // no redraw     @ios 1000bab50
    cocos2d::Value heightMeters();                                          // @ios 1000bab94
    cocos2d::Value widthMeters();                                           // @ios 1000babc4
    // width: SliderInputObject "WIDTH" (points), ptm * 3.2 .. ptm * 25.6, 0 segments;
    // height: SliderInputObject "HEIGHT", ptm * 0.512 .. ptm * 1.024, 0 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000babf4
    void setUpSprites();                                                    // @ios 1000badc4
    void createRef() override;  // iOS -create -> setUpSprites                 @ios 1000bb044

    // port: KVC (width, height, widthMeters, heightMeters, fixed), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    float _widthPoints = 0.0f;    // +0x264
    float _heightPoints = 0.0f;   // +0x268
};
