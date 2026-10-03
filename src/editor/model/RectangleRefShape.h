#pragma once
// RectangleRefShape — box shape ref (iOS `@interface RectangleRefShape : RefShape`),
// levelItemID 6000 (XML <sh t="0">).

#include "RefShape.h"

class RectangleRefShape : public RefShape
{
public:
    CREATE_FUNC(RectangleRefShape);

    // RefShape::init; canDragModify 0, canRotate 1, setWidth(ptmRatio*4.8f),
    // setHeight(ptmRatio*1.6f), levelItemID 6000.
    virtual bool init() override;                                                    // @ios 1000c5b60
    // Filled quad (4 corners of the w x h box rotated by -rotation degrees around position,
    // CGAffineTransformMakeRotation in double, corners rounded to float), fill innerColor,
    // border width 0.
    virtual void updateDrawingWithNode(cocos2d::DrawNode* node) override;            // @ios 1000c5c10
    // refRect = (-w/2, -h/2, w, h).
    virtual void updateRefRect() override;                                           // @ios 1000c5dfc
    // width/height: SliderInputObject WIDTH/HEIGHT, min ptmRatio*0.08f, max ptmRatio*80,
    // 0 segments; else RefShape's.
    virtual InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                        const cocos2d::Rect& rect) override;  // @ios 1000c5e70
};
