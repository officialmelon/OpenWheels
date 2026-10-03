#include "B2DebugDrawLayer.h"

#include "GLESDebugDraw.h"

#include "Box2D/Box2D.h"

USING_NS_CC;

// @00581dc4
B2DebugDrawLayer::B2DebugDrawLayer()
{
    _debugDraw = nullptr;
}

// @00581e1c (D1), @00581e7c (D0); thunks @00581e74, @00581ea0
B2DebugDrawLayer::~B2DebugDrawLayer()
{
    CC_SAFE_DELETE(_debugDraw);
}

// @00581ec8
B2DebugDrawLayer* B2DebugDrawLayer::create(b2World* world, float ratio)
{
    // Plain new, the result of init() is not checked and Sprite::init is never called.
    B2DebugDrawLayer* layer = new B2DebugDrawLayer(world);
    layer->init(ratio);
    layer->autorelease();
    return layer;
}

// @00581f68
bool B2DebugDrawLayer::init(float ratio)
{
    _debugDraw = new GLESDebugDraw(ratio);
    _world->SetDebugDraw(_debugDraw);
    _debugDraw->SetFlags(b2Draw::e_shapeBit | b2Draw::e_jointBit);
    return true;
}

// @00581fd8
B2DebugDrawLayer::B2DebugDrawLayer(b2World* world)
    : _world(world)
{
}

// @00582034
void B2DebugDrawLayer::draw(Renderer* renderer, const Mat4& transform, uint32_t flags)
{
    _customCommand.init(_globalZOrder, transform, flags);
    _customCommand.func = std::bind(&B2DebugDrawLayer::onDraw, this, transform, flags);
    renderer->addCommand(&_customCommand);
}

// @00582120
void B2DebugDrawLayer::onDraw(const Mat4& transform, uint32_t flags)
{
    Director* director = Director::getInstance();
    director->pushMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
    director->loadMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW, transform);
    GL::enableVertexAttribs(GL::VERTEX_ATTRIB_FLAG_POSITION);
    _world->DrawDebugData();
    director->popMatrix(MATRIX_STACK_TYPE::MATRIX_STACK_MODELVIEW);
}
