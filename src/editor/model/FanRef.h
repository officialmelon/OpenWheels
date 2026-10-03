#pragma once

#include "Special.h"

// iOS FanRef : Special (no ivars) - editor reference for the fan, level item 8 (-> Fan).
//
// Sprite: "e_1x1.png"; the create hook adds "e_fan.png" at (0, h * -0.25) and sets refRect
// (-w/2, h * -0.75, w, h). Shape count 3.
// propertyKeys: Special's defaults -> p0 xMeters, p1 yMeters, p2 angle, p3 fixed, p4 sleeping
// (Fan::init reads p0..p2 only).
// UI keys: x, y, angle.
class FanRef : public Special
{
public:
    CREATE_FUNC(FanRef);

    bool init() override;                                                   // @ios 1000b8444
    void onEnter() override;                                                // @ios 1000b84ac
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000b8500
    void createRef() override;  // iOS -create                                 @ios 1000b857c
};
