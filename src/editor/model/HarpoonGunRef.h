#pragma once

#include "Special.h"

class InputObject;

// iOS HarpoonGunRef : Special (instanceSize 0x288) - editor reference for the harpoon gun,
// level item 15 (-> HarpoonGun).
//
// Sprite: "e_1x1.png"; setUpSprites (create hook) adds "e_harpoon_base.png" (_base),
// "e_harpoon_rope.png" (rope, visible only with the anchor) and "e_harpoon_turret.png"
// (_turret, rotated by turretAngle). Shape count 11 with the anchor, 4 without.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 angle, p3 useAnchor, p4 fixedTurret,
// p5 turretAngle, p6 triggerFiring, p7 startDeactivated (HarpoonGun::init: p3/p4 bools,
// p5 float degrees, p6 bool, p7 bool "_disabled").
// Defaults: useAnchor 1, everything else 0.
// UI keys: x, y, angle, useAnchor, fixedTurret, [turretAngle only when fixedTurret],
// triggerFiring, startDeactivated.
class HarpoonGunRef : public Special
{
public:
    CREATE_FUNC(HarpoonGunRef);

    bool init() override;                                                   // @ios 1000d1ba4
    void onEnter() override;                                                // @ios 1000d1cdc
    // Posts ref_ui_keys_will_change / ref_ui_keys_changed around the change (UI key list
    // depends on it).
    void setFixedTurret(bool fixedTurret);                                  // @ios 1000d1d30
    bool fixedTurret();                                                     // @ios 1000d1db0
    void setTriggerFiring(bool triggerFiring);                              // @ios 1000d1dc0
    bool triggerFiring();                                                   // @ios 1000d1dd0
    void setStartDeactivated(bool startDeactivated);                        // @ios 1000d1de0
    bool startDeactivated();                                                // @ios 1000d1df0
    void setTurretAngle(int turretAngle);                                   // @ios 1000d1e00
    void updateBoundingBox();                                               // @ios 1000d1e58
    int turretAngle();                                                      // @ios 1000d1edc
    // Sets the shape count (11 / 4) and the rope's visibility.
    void setUseAnchor(const cocos2d::Value& useAnchor);                     // @ios 1000d1eec
    bool useAnchor();                                                       // @ios 1000d1f60
    void setUpSprites();                                                    // @ios 1000d1f70
    void createRef() override;  // iOS -create -> setUpSprites                 @ios 1000d2124
    // useAnchor: SwitchInputObject "USE ANCHOR"; fixedTurret: SwitchInputObject "FIXED TURRET";
    // turretAngle: SliderInputObject "TURRET ANGLE", -110..110, 220 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000d2128
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000d235c

    // port: KVC (useAnchor, fixedTurret, turretAngle, triggerFiring, startDeactivated).
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    bool anchor = false;                    // +0x264
    cocos2d::Sprite* rope = nullptr;        // +0x268
    cocos2d::Sprite* _turret = nullptr;     // +0x270
    cocos2d::Sprite* _base = nullptr;       // +0x278
    bool _fixedTurret = false;              // +0x280
    bool _triggerFiring = false;            // +0x281
    bool _startDeactivated = false;         // +0x282
    int _turretAngle = 0;                   // +0x284
};
