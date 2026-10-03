#include "SoccerBall.h"

#include "LevelB2D.h"
#include "LevelDataElement.h"

USING_NS_CC;

// @00613b24 (D0; the complete destructor is LevelItem's)
SoccerBall::~SoccerBall()
{
}

// @006138ac
bool SoccerBall::init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset)
{
    loadSpriteFrames(LevelItemTextureIdLevelItems);

    float y = 0.0f;
    float x = 0.0f;
    element->floatAttribute("p0", &x);
    element->floatAttribute("p1", &y);
    getLevel()->convertPositionData(&x, &y);

    Sprite* sprite = Sprite::createWithSpriteFrameName("soccerball.png");
    sprite->setPosition(x * getPtm(), y * getPtm());
    getLevelItemsNode()->addChild(sprite);

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position.Set(x, y);
    bodyDef.userData = sprite;

    b2CircleShape shape;
    shape.m_radius = 0.16f;

    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.friction = 0.1f;
    fixtureDef.restitution = 0.5f;
    fixtureDef.density = 0.5f;
    fixtureDef.filter.categoryBits = 8;

    b2Body* body = getWorld()->CreateBody(&bodyDef);
    body->CreateFixture(&fixtureDef);
    getLevel()->addToPaintBody(body);
    return true;
}
