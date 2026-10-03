#include "FanRef.h"

USING_NS_CC;

// @ios 1000b8444
bool FanRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    setLevelItemID(8);
    setShapeCount(3);
    return true;
}

// @ios 1000b84ac
void FanRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000b8500
std::vector<std::string> FanRef::propertyKeysForUI()
{
    return {"x", "y", "angle"};
}

// @ios 1000b857c
void FanRef::createRef()
{
    Sprite* fan = Sprite::createWithSpriteFrameName("e_fan.png");
    fan->setScale(editorArtScale());  // port: art at its iOS point size
    addChild(fan);
    // textureRect in iOS points.
    float width = fan->getTextureRect().size.width * editorArtScale();
    float height = fan->getTextureRect().size.height * editorArtScale();
    fan->setPosition(Vec2(0.0f, height * -0.25f));
    setRefRect(Rect(width * -0.5f, height * -0.75f, width, height));
}
