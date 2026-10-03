#pragma once
// RefShape — editor ref for a primitive shape (iOS `@interface RefShape : Special`), base of
// RectangleRefShape (6000), CircleRefShape (6001), TriangleRefShape (6002).
// Written to XML as <sh t="levelItemID-6000" i="interactive" p0..p11 />.
//
// propertyKeys (p0..p11): xMeters yMeters widthMeters heightMeters angle fixed sleeping density
//                         color outlineColor shapeOpacity collision
// propertyKeysForUI: interactive: x y width height angle innerRed innerGreen innerBlue
//                                 shapeOpacity interactive fixed sleeping density collision
//                    otherwise:   x y width height angle innerRed innerGreen innerBlue
//                                 shapeOpacity interactive collision
// Defaults (init): sprite frame "e_1x1.png"; colour/opacity from the "last used" statics
// (initially 61,136,199 / 100, updated by setInnerRed/Green/Blue and setShapeOpacity — not by
// setColor), outlineColor -1, density 1, collision 1, fixed 1, interactive 1, sleeping 0,
// shapeCount 1, artCount 0. Shape outlines are drawn by EditorSpriteBatchNode through
// updateDrawingWithNode (the sprite itself is the 1x1 placeholder).

#include "Special.h"

class RefShape : public Special
{
public:
    // Abstract on iOS in practice (never instantiated directly; no CREATE_FUNC).
    virtual bool init() override;                                                    // @ios 1000da374
    // "i" -> _interactive directly (default 1 when absent; no shape/art count update); an old
    // file's "p12" is moved to "p11"; then Special::setProperties.
    virtual void setProperties(const cocos2d::ValueMap& properties) override;        // @ios 1000da5a0
    // Special::onEnter, then setShapeCount(shapeCount()) (re-posts SHAPE_COUNT_UPDATE).
    virtual void onEnter() override;                                                 // @ios 1000da698

    // innerColor.a = v * 0.01f; also updates the static default opacity.
    void setShapeOpacity(unsigned int shapeOpacity);                                 // @ios 1000da6ec
    unsigned int shapeOpacity();                                                     // @ios 1000da728
    void setWidthMeters(float widthMeters);         // + updateRefRect                // @ios 1000da738
    void setHeightMeters(float heightMeters);       // + updateRefRect                // @ios 1000da748
    // Only on change: post ref_ui_keys_will_change, Special::setInteractive,
    // ref_ui_keys_changed, setShapeCount(i), setArtCount(!i).
    virtual void setInteractive(const cocos2d::Value& interactive) override;         // @ios 1000da758
    virtual float width() override;                 // ptmRatio * widthMeters()           // @ios 1000da81c
    virtual float height() override;                // ptmRatio * heightMeters()          // @ios 1000da850
    virtual void setWidth(float width) override;    // setWidthMeters(width / ptmRatio)   // @ios 1000da884
    virtual void setHeight(float height) override;                                   // @ios 1000da89c
    // Each sets the component, the static default and innerColor.<c> = v * 0.003921569f.
    void setInnerRed(unsigned int innerRed);                                         // @ios 1000da8b4
    void setInnerGreen(unsigned int innerGreen);                                     // @ios 1000da8ec
    void setInnerBlue(unsigned int innerBlue);                                       // @ios 1000da928
    // 0xRRGGBB -> innerColor rgb and inner components ((unsigned)(c * 255.0f)); alpha and the
    // statics untouched. NB: on iOS this selector also overrides CCSprite -setColor:(ccColor3B);
    // here it is an overload next to Node::setColor(const Color3B&).
    using Special::setColor;
    void setColor(unsigned int color);                                               // @ios 1000da964
    unsigned int color();                           // hexFromColor(innerColor)           // @ios 1000da9e0
    // (int)(b*255.0f) + (int)(g*255.0f)*0x100 + (int)(r*255.0f)*0x10000 (all float math).
    int hexFromColor(const cocos2d::Color4F& color);                                 // @ios 1000da9f8
    // ((rgb>>16)&0xff, (rgb>>8)&0xff, rgb&0xff) * 0.003921569f, alpha 1.
    cocos2d::Color4F ccColorFromRGB(long long rgb);                                  // @ios 1000daa24
    virtual std::vector<std::string> propertyKeysForUI() override;                   // @ios 1000daa5c
    // width/height: InputObject WIDTH/HEIGHT (display field); innerRed: ColorInputObject
    // label COLOR, colour label "r", 0..255, 254 segments; innerGreen/innerBlue: same with no
    // label, "g"/"b"; shapeOpacity: ColorInputObject no label, "a", 0..100, 99 segments;
    // collision: SliderInputObject COLLISION 1..6, 5 segments; density: InputObject DENSITY
    // (display field), min 0.1 max 100; else Special's.
    virtual InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                        const cocos2d::Rect& rect) override;  // @ios 1000dabd4
    void setCollision(unsigned int collision);                                       // @ios 1000db034
    unsigned int collision();                                                        // @ios 1000db044
    virtual void updateRefRect();                   // no-op; subclasses set refRect      // @ios 1000db054
    virtual void updateDrawingWithNode(cocos2d::DrawNode* node) override;  // no-op       // @ios 1000db058
    float widthMin();                                                                // @ios 1000db05c
    void setWidthMin(float widthMin);                                                // @ios 1000db06c
    float widthMax();                                                                // @ios 1000db07c
    void setWidthMax(float widthMax);                                                // @ios 1000db08c
    float heightMin();                                                               // @ios 1000db09c
    void setHeightMin(float heightMin);                                              // @ios 1000db0ac
    float heightMax();                                                               // @ios 1000db0bc
    void setHeightMax(float heightMax);                                              // @ios 1000db0cc
    float widthMeters();                                                             // @ios 1000db0dc
    float heightMeters();                                                            // @ios 1000db0ec
    float density();                                                                 // @ios 1000db0fc
    void setDensity(float density);                                                  // @ios 1000db10c
    unsigned int innerRed();                                                         // @ios 1000db11c
    unsigned int innerGreen();                                                       // @ios 1000db12c
    unsigned int innerBlue();                                                        // @ios 1000db13c
    float outlineColor();                                                            // @ios 1000db14c
    void setOutlineColor(float outlineColor);                                        // @ios 1000db15c
    cocos2d::Color4F innerColor();                                                   // @ios 1000db16c
    void setInnerColor(const cocos2d::Color4F& innerColor);                          // @ios 1000db184

    // port: KVC — widthMeters heightMeters density color outlineColor shapeOpacity collision
    // innerRed innerGreen innerBlue width height (+ Special's).
    virtual cocos2d::Value valueForKey(const std::string& key) override;
    virtual void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    RefShape();

    // iOS ivars (RefShape : Special, instanceSize 0x2a8)
    unsigned int _shapeOpacityWorkaround;   // _shapeOpacityWorkaround  0..100
    unsigned int _collision;                // _collision               1..6
    float _widthMin;                        // _widthMin   (never set; 0)
    float _widthMax;                        // _widthMax
    float _heightMin;                       // _heightMin
    float _heightMax;                       // _heightMax
    float _widthMeters;                     // _widthMeters
    float _heightMeters;                    // _heightMeters
    float _density;                         // _density
    unsigned int _innerRed;                 // _innerRed    0..255
    unsigned int _innerGreen;               // _innerGreen
    unsigned int _innerBlue;                // _innerBlue
    float _outlineColor;                    // _outlineColor (-1)
    cocos2d::Color4F _innerColor;           // _innerColor  ccColor4F
};
