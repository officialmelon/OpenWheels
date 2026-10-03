#pragma once
// TriangleRefShape — isosceles triangle ref (iOS `@interface TriangleRefShape : RefShape`),
// levelItemID 6002 (XML <sh t="2">). Apex at +2h/3, base at -h/3 (centroid at the position).

#include "RefShape.h"

class TriangleRefShape : public RefShape
{
public:
    CREATE_FUNC(TriangleRefShape);

    // RefShape::init; canDragModify 0, canRotate 1, setHeight/setWidth(ptmRatio*3.2f),
    // levelItemID 6002.
    virtual bool init() override;                                                    // @ios 10001b058
    // Filled triangle (0, 2h/3), (-w/2, -h/3), (w/2, -h/3) rotated by -rotation degrees around
    // position (0.6666667f / -0.33333334f factors), fill innerColor, border width 0.
    virtual void updateDrawingWithNode(cocos2d::DrawNode* node) override;            // @ios 10001b104
    // width: SliderInputObject WIDTH, min ptmRatio*0.08f, max ptmRatio*24;
    // height: SliderInputObject HEIGHT, min ptmRatio*0.24f, max ptmRatio*72; 0 segments;
    // else RefShape's.
    virtual InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                        const cocos2d::Rect& rect) override;  // @ios 10001b270
    // refRect = (-w/2, -h/3, w, h).
    virtual void updateRefRect() override;                                           // @ios 10001b444
};
