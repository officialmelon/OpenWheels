#include "Test.h"

USING_NS_CC;

// @00636afc
Test::Test()
{
}

// @00636b2c (D1), @00636b30 (D0)
Test::~Test()
{
}

// @00636b54
Test* Test::create()
{
    Test* layer = new (std::nothrow) Test();
    if (layer && layer->init())
    {
        layer->autorelease();
        return layer;
    }
    delete layer;
    return nullptr;
}

// @00636be0
Scene* Test::createScene()
{
    Scene* scene = Scene::create();
    Test* layer = Test::create();
    scene->addChild(layer);
    return scene;
}

// @00636c88
bool Test::init()
{
    // Layer::init() is not called.
    Sprite* sprite = Sprite::create("characters/wheelchair_guy_sprites.png");
    sprite->setPosition(100.0f, 100.0f);
    addChild(sprite);
    return true;
}
