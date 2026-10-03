#pragma once

#include "Special.h"

class InputObject;

// iOS SpikesRef : Special (instanceSize 0x274) - editor reference for the spike strip,
// level item 6 (-> Spikes).
//
// Sprite: "e_1x1.png"; setUpSprites (create hook) lays out numSpikes "e_spike.png",
// _spikeWidth = ptm * 0.24 apart, and sets refRect. canDragModify. Shape count 2.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 angle, p3 fixed, p4 num, p5 sleeping
// (Spikes::init: p3 bool, p4 numSpikes int, p5 bool).
// Defaults: num 20 (min 20, max 150), fixed 1, sleeping 0.
// UI keys: x, y, angle, fixed, num, [sleeping only when !fixed].
class SpikesRef : public Special
{
public:
    CREATE_FUNC(SpikesRef);

    bool init() override;                                                   // @ios 1000b4258
    void onEnter() override;                                                // @ios 1000b43f0
    void setNum(const cocos2d::Value& num);   // intValue, then setUpSprites   @ios 1000b4444
    unsigned int num();                                                     // @ios 1000b4478
    // Only when the value changes: posts ref_ui_keys_will_change, Special::setFixed,
    // posts ref_ui_keys_changed.
    void setFixed(const cocos2d::Value& fixed) override;                    // @ios 1000b4488
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000b452c
    // num: SliderInputObject "SPIKES", initial num, minNumSpikes..maxNumSpikes,
    // segments max - min (130); else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000b45c4
    void setUpSprites();                                                    // @ios 1000b46f4
    void createRef() override;  // iOS -create -> setUpSprites                 @ios 1000b4854

    // port: KVC (num, fixed), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    unsigned int numSpikes = 0;      // +0x264
    unsigned int minNumSpikes = 0;   // +0x268
    unsigned int maxNumSpikes = 0;   // +0x26c
    float _spikeWidth = 0.0f;        // +0x270
};
