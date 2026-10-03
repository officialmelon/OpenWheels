#include "WreckingBallRef.h"

#include "SliderInputObject.h"

#include <cmath>

USING_NS_CC;

// @ios 1000f5e40
bool WreckingBallRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    setCanDragModify(false);
    setCanRotate(true);
    _fixed = true;
    _ropeLength = 350.0f;
    // iOS reads Session.ptmRatio for every use.
    _scaledRopeLength = sessionPtmRatio() * 4.0f;
    ballWidth = sessionPtmRatio() * 2.4f;
    const float ptmA = sessionPtmRatio();
    const float chainOverlap = sessionPtmRatio() * 0.016f;
    const float ptmB = sessionPtmRatio();
    anchorHeight = sessionPtmRatio() * 0.512f;

    const float anchorOffset = ptmB * 0.144f;
    // port: art children shown at their iOS point size (editorArtScale); `ball` is an e_1x1
    // container and keeps scale 1.
    const float art = editorArtScale();
    Sprite* anchor = Sprite::createWithSpriteFrameName("e_wreckingball_anchor.png");
    anchor->setScale(art);
    anchor->setPosition(Vec2(chainOverlap - anchorOffset, anchorHeight * 0.5f));
    addChild(anchor);
    anchor = Sprite::createWithSpriteFrameName("e_wreckingball_anchor.png");
    anchor->setPosition(Vec2(anchorOffset - chainOverlap, anchorHeight * 0.5f));
    anchor->setScaleY(art);
    anchor->setScaleX(-1.0f * art);
    addChild(anchor);

    ball = Sprite::createWithSpriteFrameName("e_1x1.png");
    ball->setPosition(Vec2(0.0f, _ropeLength * (-0.016f * sessionPtmRatio())));
    addChild(ball);

    const float halfY = ptmA * 0.2f;
    Sprite* half = Sprite::createWithSpriteFrameName("e_wreckingball.png");
    half->setScale(art);
    half->setPosition(Vec2(std::fma(-ballWidth, 0.25f, chainOverlap), halfY));
    ball->addChild(half);
    half = Sprite::createWithSpriteFrameName("e_wreckingball.png");
    half->setPosition(Vec2(std::fma(ballWidth, 0.25f, -chainOverlap), halfY));
    half->setScaleY(art);
    half->setScaleX(-1.0f * art);
    ball->addChild(half);

    setLevelItemID(7);
    updateRefRect();
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(), {"xMeters", "yMeters", "ropeLengthMeters"});
    setShapeCount(3);
    return true;
}

// @ios 1000f6184
void WreckingBallRef::setRotation(float /*rotation*/)
{
}

// @ios 1000f6188
void WreckingBallRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000f61dc
std::vector<std::string> WreckingBallRef::propertyKeysForUI()
{
    return {"x", "y", "ropeLength"};
}

// @ios 1000f6220
void WreckingBallRef::updateRefRect()
{
    float x = ballWidth * -0.5f;
    float y = std::fma(_ropeLength, -0.016f * sessionPtmRatio(), ballWidth * -0.5f);
    float width = ballWidth;
    float height = std::fma(ballWidth, 0.5f, _ropeLength * (0.016f * sessionPtmRatio()));
    height = height + anchorHeight;
    setRefRect(Rect(x, y, width, height));
}

// @ios 1000f6308
void WreckingBallRef::updateSprites()
{
    ball->setPosition(Vec2(0.0f, _ropeLength * (-0.016f * sessionPtmRatio())));
}

// @ios 1000f6370
void WreckingBallRef::setRopeLength(const Value& ropeLength)
{
    KeyValueChange kvo(this, "ropeLength");
    _ropeLength = kvcFloat(ropeLength);
    _scaledRopeLength = _ptmRatio * std::fma(_ropeLength, 0.016f, -1.6f);
    updateSprites();
    updateRefRect();
}

// @ios 1000f63e8
Value WreckingBallRef::ropeLength()
{
    return Value(_ropeLength);
}

// @ios 1000f6404
Value WreckingBallRef::ropeLengthMeters()
{
    return Value(_ropeLength * 0.016f);
}

// @ios 1000f642c
void WreckingBallRef::setRopeLengthMeters(const Value& ropeLengthMeters)
{
    KeyValueChange kvo(this, "ropeLengthMeters");
    // Stores only: a loaded level keeps the default rope drawing until the slider is used
    // (iOS behaviour - there is no create hook to redraw).
    _ropeLength = kvcFloat(ropeLengthMeters) * 62.5f;
}

// @ios 1000f6468
InputObject* WreckingBallRef::inputObjectForPropertyWithRect(const std::string& property,
                                                            const Rect& rect)
{
    if (property == "ropeLength")
    {
        return SliderInputObject::create(rect, "ROPELENGTH", "ropeLength", _ropeLength, 200.0f,
                                         1000.0f, 800);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000f6584
float WreckingBallRef::scaledRopeLength()
{
    return _scaledRopeLength;
}

Value WreckingBallRef::valueForKey(const std::string& key)
{
    if (key == "ropeLength")
    {
        return ropeLength();
    }
    if (key == "ropeLengthMeters")
    {
        return ropeLengthMeters();
    }
    return Special::valueForKey(key);
}

void WreckingBallRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "ropeLength")
    {
        setRopeLength(value);
    }
    else if (key == "ropeLengthMeters")
    {
        setRopeLengthMeters(value);
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
