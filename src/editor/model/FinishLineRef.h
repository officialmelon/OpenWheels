#pragma once

#include "Special.h"

// iOS FinishLineRef : Special (no ivars) - editor reference for the finish line, level item 9
// (-> FinishLine).
//
// Sprite: "e_1x1.png"; the create hook builds pole ("e_finishline_pole.png", scaled to
// ptm * 3.328 high), cap, 20 banner segments ("e_finishline_banner.png", scaleX 1.05,
// ptm * 0.32 apart) and the flag, then sets refRect. canDragModify 0, canRotate 1, but
// setRotation is a no-op. Shape count 2.
// propertyKeys -> XML: p0 xMeters, p1 yMeters (FinishLine::init reads p0, p1).
// UI keys: x, y.
class FinishLineRef : public Special
{
public:
    CREATE_FUNC(FinishLineRef);

    bool init() override;                                                   // @ios 1000f0e34
    void setRotation(float rotation) override;  // no-op                       @ios 1000f0f34
    void onEnter() override;                                                // @ios 1000f0f38
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000f0f8c
    void createRef() override;  // iOS -create                                 @ios 1000f0ff8
};
