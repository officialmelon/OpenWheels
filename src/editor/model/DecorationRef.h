#pragma once
// DecorationRef — decoration item (iOS `@interface DecorationRef : Special`), levelItemID 5004.
// Not listed in the iOS item registry (Settings levelItems), so the editor UI cannot add one;
// the class is still complete and round-trips if a level contains t=5004.
//
// propertyKeys: xMeters yMeters angle decorationType interactive fixed
// propertyKeysForUI: x y angle decorationType interactive [+ fixed if interactive
//                    [+ sleeping if !fixed]]
// shapeCountDict: "decoration_<n>" -> number of Box2D shapes of that body in the bundle's
// PhysicsEditor file pe_objects-hd.plist (POLYGON fixtures count their polygons, CIRCLE 1).
// Art: child sprite frame "e_decoration_<n>.png".

#include "Special.h"

class DecorationRef : public Special
{
public:
    CREATE_FUNC(DecorationRef);

    // initWithSpriteFrameName("e_1x1.png"); canDragModify 0, canRotate 1, decorationType @0,
    // refreshSprite, levelItemID 5004, propertyKeys as above, build shapeCountDict,
    // setShapeCount(dict["decoration_0"]), setArtCount(0).
    virtual bool init() override;                                                    // @ios 1000a6344
    // Special::onEnter, then re-post shape and art counts.
    virtual void onEnter() override;                                                 // @ios 1000a6710
    virtual ~DecorationRef();                                                        // @ios 1000a6778 (dealloc)
    virtual std::vector<std::string> propertyKeysForUI() override;                   // @ios 1000a67d8
    // Always: post ref_ui_keys_will_change, _fixed = boolValue, post ref_ui_keys_changed.
    virtual void setFixed(const cocos2d::Value& fixed) override;                     // @ios 1000a6894
    // Always: post will_change; _interactive = boolValue; false -> shapeCount 0, artCount 1;
    // true -> artCount 0, shapeCount = dict["decoration_<type>"]; post changed.
    virtual void setInteractive(const cocos2d::Value& interactive) override;         // @ios 1000a6904
    void refreshShapeCount();                       // shapeCount = dict[type]            // @ios 1000a69fc
    // No-op when intValue unchanged; else store the number, update shape count, refreshSprite.
    void setDecorationType(const cocos2d::Value& decorationType);                    // @ios 1000a6a78
    // Replace child sprite with "e_decoration_<type>.png"; refRect = sprite contentSize
    // grown by 1 on each side, centred: (-1 - w*0.5, -1 - h*0.5, w + 2, h + 2) (double math).
    void refreshSprite();                                                            // @ios 1000a6b40
    // decorationType: SliderInputObject TYPE, initial floatValue, min 0, max count-1,
    // count-1 segments; else Special's.
    virtual InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                        const cocos2d::Rect& rect) override;  // @ios 1000a6c08
    cocos2d::Value decorationType();                // the stored NSNumber                // @ios 1000a6d64

    // port: KVC — decorationType (+ Special's).
    virtual cocos2d::Value valueForKey(const std::string& key) override;
    virtual void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    DecorationRef();

    // iOS ivars (DecorationRef : Special, instanceSize 0x280)
    cocos2d::Value _decorationType;         // decorationType  NSNumber (int or float as set)
    cocos2d::Sprite* _sprite;               // sprite          CCSprite child
    cocos2d::ValueMap _shapeCountDict;      // shapeCountDict  NSDictionary<NSString, NSNumber>
};
