#include "Token.h"

#include "LevelDataElement.h"

USING_NS_CC;

// @00636ed0 (D0; the complete destructor is LevelItem's)
Token::~Token()
{
}

// @00636d84
// Stub on Android: the attributes are read into locals and never used.
bool Token::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);

    float y = 0.0f;
    float x = 0.0f;
    float rotation = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    element->floatAttribute("p2", &rotation);
    return true;
}
