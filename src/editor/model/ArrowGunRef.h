#pragma once

#include "Special.h"

class InputObject;

// iOS ArrowGunRef : Special (instanceSize 0x26c) - editor reference for the arrow gun,
// level item 29 (LevelB2D::addSpecial -> ArrowGun).
//
// Sprite: "e_1x1.png" with a child "e_arrow_gun.png" (anchor (0.5, 0), y = ptm * -0.128 + 0.5);
// refRect = that child's texture rect shifted by (-w/2, y). Shape count 7.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 angle, p3 fixed, p4 rateOfFire,
// p5 dontShootPlayer (ArrowGun::init reads p3 bool, p4 int, p5 bool).
// Defaults: fixed = 1, rateOfFire = 5, dontShootPlayer = 0.
// UI keys: x, y, angle, fixed, rateOfFire (dontShootPlayer is saved but has no input).
class ArrowGunRef : public Special
{
public:
    CREATE_FUNC(ArrowGunRef);

    bool init() override;                                                   // @ios 10003fc08
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 10003fdfc
    // rateOfFire: SliderInputObject "RATE OF FIRE", 1..10, 9 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 10003fe98
    // Re-posts the shape count (Special::setShapeCount(shapeCount())).
    void onEnter() override;                                                // @ios 10003ffbc

    unsigned int rateOfFire();                                              // @ios 100040010
    void setRateOfFire(unsigned int rateOfFire);                            // @ios 100040020
    bool dontShootPlayer();                                                 // @ios 100040030
    void setDontShootPlayer(bool dontShootPlayer);                          // @ios 100040040

    // port: KVC for the keys above (NSObject valueForKey:/setValue:forKey:), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    bool _dontShootPlayer = false;   // +0x264
    unsigned int _rateOfFire = 0;    // +0x268
};
