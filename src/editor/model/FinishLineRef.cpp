#include "FinishLineRef.h"

#include <cmath>

USING_NS_CC;

// @ios 1000f0e34
bool FinishLineRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    setCanDragModify(false);
    setCanRotate(true);
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(), {"xMeters", "yMeters"});
    setLevelItemID(9);
    setShapeCount(2);
    return true;
}

// @ios 1000f0f34
void FinishLineRef::setRotation(float /*rotation*/)
{
}

// @ios 1000f0f38
void FinishLineRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000f0f8c
std::vector<std::string> FinishLineRef::propertyKeysForUI()
{
    return {"x", "y"};
}

// @ios 1000f0ff8
void FinishLineRef::createRef()
{
    // iOS re-reads Session.ptmRatio for each use.
    const float ptm = sessionPtmRatio();
    const float bannerStep = ptm * 0.319999993f;
    const float left = ptm * -3.19999981f;
    const float poleX = std::fma(sessionPtmRatio(), 0.0560000017f, left);

    Sprite* pole = Sprite::createWithSpriteFrameName("e_finishline_pole.png");
    // textureRect height in iOS points; the pole is stretched to ptm * 3.328 points.
    float poleHeight = pole->getTextureRect().size.height * editorArtScale();
    pole->setScaleX(editorArtScale());
    pole->setScaleY(sessionPtmRatio() * 3.32800007f / poleHeight * editorArtScale());
    pole->setPosition(Vec2(poleX, sessionPtmRatio() * 1.91999996f));
    addChild(pole);

    Sprite* cap = Sprite::createWithSpriteFrameName("e_finishline_cap.png");
    cap->setScale(editorArtScale());  // port: art at its iOS point size
    cap->setPosition(Vec2(std::fma(sessionPtmRatio(), 0.0560000017f, left),
                          sessionPtmRatio() * 3.5999999f));
    addChild(cap);

    float bannerX = ptm * -3.03999972f;
    for (int i = 20; i != 0; --i)
    {
        Sprite* banner = Sprite::createWithSpriteFrameName("e_finishline_banner.png");
        banner->setPosition(Vec2(bannerX, 0.0f));
        banner->setScaleY(editorArtScale());
        banner->setScaleX(1.04999995f * editorArtScale());
        bannerX = bannerX + bannerStep;
        addChild(banner);
    }

    const float refWidth = ptm * 6.39999962f;
    Sprite* flag = Sprite::createWithSpriteFrameName("e_finishline_flag.png");
    flag->setScale(editorArtScale());
    flag->setPosition(Vec2(sessionPtmRatio() * -2.28800011f, sessionPtmRatio() * 3.03999996f));
    addChild(flag);

    float refHeight = sessionPtmRatio() * 4.0f;
    float ptmA = sessionPtmRatio();
    float ptmB = sessionPtmRatio();
    float refY = std::fma(ptmA, -0.159999996f, ptmB * -0.159999996f);
    setRefRect(Rect(left, refY, refWidth, refHeight));
}
