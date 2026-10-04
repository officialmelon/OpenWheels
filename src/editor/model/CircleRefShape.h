#pragma once
// CircleRefShape — circle shape ref (iOS `@interface CircleRefShape : RefShape`), levelItemID
// 6001 (XML <sh t="1">). width = diameter, height kept equal; no rotation or height input.

#include "RefShape.h"

class CircleRefShape : public RefShape
{
public:
    CREATE_FUNC(CircleRefShape);

    // RefShape::init; canDragModify 0, canRotate 1, setHeight/setWidth(ptmRatio * 3.2f),
    // levelItemID 6001.
    virtual bool init() override;                                                    // @ios 1000b3d40
    float innerCutout();                            // 0 (unused)                         // @ios 1000b3dec
    void setInnerCutout(float innerCutout);         // no-op                              // @ios 1000b3df4
    // log "properties: %@", then RefShape::setProperties.
    virtual void setProperties(const cocos2d::ValueMap& properties) override;        // @ios 1000b3df8
    // RefShape's list minus "height" and "angle".
    virtual std::vector<std::string> propertyKeysForUI() override;                   // @ios 1000b3e50
    // width: SliderInputObject DIAMETER, initial width(), min ptmRatio*0.08f,
    // max ptmRatio*80, 0 segments; else RefShape's.
    virtual InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                        const cocos2d::Rect& rect) override;  // @ios 1000b3ec8
    // drawDot(position, width()*0.5f, innerColor).
    virtual void updateDrawingWithNode(cocos2d::DrawNode* node) override;            // @ios 1000b4010
    // refRect = (-w/2, -h/2, w, h).
    virtual void updateRefRect() override;                                           // @ios 1000b4098
    // Unrotated square around position: (pos.x - w/2, pos.y - w/2, w, w).
    virtual cg::Rect refBoundingBox() override;                                 // @ios 1000b410c

    // port: KVC — innerCutout (+ RefShape's).
    virtual cocos2d::Value valueForKey(const std::string& key) override;
    virtual void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    float _innerCutout = 0.0f;  // EDITOR (browser features, PC addition)
};
