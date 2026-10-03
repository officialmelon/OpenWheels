#pragma once

#include "Special.h"

// iOS VanRef : Special (no ivars) - editor reference for the van, level item 0 (-> Van).
//
// Sprite: "e_1x1.png" with two mirrored "e_van.png" halves (ptm * 0.648 apart), two
// "e_van_wheel.png" (+-ptm * 0.928, ptm * -0.768) and "e_van_label.png"; refRect is set in init
// from ptm-scaled constants. canDragModify 0. Shape count 3.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 angle, p3 sleeping, p4 interactive
// (Van::init: p3/p4 bools).
// UI keys: x, y, angle, interactive, [sleeping only when interactive].
class VanRef : public Special
{
public:
    CREATE_FUNC(VanRef);

    bool init() override;                                                   // @ios 1000ec584
    // Always posts ref_ui_keys_will_change, Special::setInteractive, ref_ui_keys_changed.
    void setInteractive(const cocos2d::Value& interactive) override;        // @ios 1000ec8e0
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000ec964
    void onEnter() override;                                                // @ios 1000eca40
};
