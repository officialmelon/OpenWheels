#include "Chain.h"

#include "LevelDataElement.h"

USING_NS_CC;

// @00589594 (D0; the complete destructor is LevelItem's)
Chain::~Chain()
{
}

// @00589448
// Stub on Android: the attributes are read into locals and never used.
bool Chain::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
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
