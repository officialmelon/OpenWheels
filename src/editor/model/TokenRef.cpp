#include "TokenRef.h"

USING_NS_CC;

// @ios 1000bccc8
bool TokenRef::init()
{
    if (!Special::initWithSpriteFrameName("e_coin.png"))
    {
        return false;
    }
    setCanDragModify(false);
    setCanRotate(true);
    setLevelItemID(31);
    setShapeCount(1);
    return true;
}

// @ios 1000bcd48
void TokenRef::setRotation(float /*rotation*/)
{
}

// @ios 1000bcd4c
void TokenRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000bcda0
std::vector<std::string> TokenRef::propertyKeysForUI()
{
    return {"xMeters", "yMeters"};
}

// @ios 1000bce14
void TokenRef::createRef()
{
    // textureRect in iOS points.
    const Size size = getTextureRect().size * editorArtScale();
    setRefRect(Rect(0.0f, 0.0f, size.width, size.height));
}
