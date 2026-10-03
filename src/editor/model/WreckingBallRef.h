#pragma once

#include "Special.h"

class InputObject;

// iOS WreckingBallRef : Special (instanceSize 0x280) - editor reference for the wrecking ball,
// level item 7 (-> WreckingBall).
//
// Sprite: "e_1x1.png" with two mirrored "e_wreckingball_anchor.png" halves, a rope child
// ("e_1x1.png") and `ball` (two mirrored "e_wreckingball.png" halves) hanging ropeLength below;
// updateRefRect spans anchor to ball. canDragModify 0, canRotate 1, but setRotation is a no-op.
// Shape count 3.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 ropeLengthMeters (WreckingBall::init: p2 length
// in meters). ropeLength is in Flash pixels: ropeLengthMeters = ropeLength * 0.016, setter * 62.5.
// Defaults: ropeLength 350 (5.6 m), scaledRopeLength = ptm * (ropeLength * 0.016 - 1.6),
// ballWidth = ptm * 2.4, anchorHeight = ptm * 0.512.
// UI keys: x, y, ropeLength.
// init also sets fixed = 1 (not a key). No create hook: setRopeLengthMeters only stores, so a
// loaded ref keeps the default rope drawing until ropeLength is edited (iOS behaviour).
class WreckingBallRef : public Special
{
public:
    CREATE_FUNC(WreckingBallRef);

    bool init() override;                                                   // @ios 1000f5e40
    void setRotation(float rotation) override;  // no-op                       @ios 1000f6184
    void onEnter() override;                                                // @ios 1000f6188
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000f61dc
    void updateRefRect();                                                   // @ios 1000f6220
    void updateSprites();                                                   // @ios 1000f6308
    // Sets ropeLength and scaledRopeLength, then updateSprites + updateRefRect.
    void setRopeLength(const cocos2d::Value& ropeLength);                   // @ios 1000f6370
    cocos2d::Value ropeLength();                                            // @ios 1000f63e8
    cocos2d::Value ropeLengthMeters();                                      // @ios 1000f6404
    // Stores only (no redraw).
    void setRopeLengthMeters(const cocos2d::Value& ropeLengthMeters);       // @ios 1000f642c
    // ropeLength: SliderInputObject "ROPELENGTH", 200..1000, 800 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000f6468
    float scaledRopeLength();                                               // @ios 1000f6584

    // port: KVC (ropeLength, ropeLengthMeters), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    cocos2d::Sprite* ball = nullptr;   // +0x268
    float _ropeLength = 0.0f;          // +0x270  iOS ivar "ropeLength" (clashes with ropeLength())
    float ballWidth = 0.0f;            // +0x274
    float anchorHeight = 0.0f;         // +0x278
    float _scaledRopeLength = 0.0f;    // +0x27c  iOS ivar "scaledRopeLength" (clashes with getter)
};
