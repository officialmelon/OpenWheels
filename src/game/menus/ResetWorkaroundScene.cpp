#include "ResetWorkaroundScene.h"

#include "Gameplay.h"

USING_NS_CC;

// @0064c8c4
Scene* ResetWorkaroundScene::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(ResetWorkaroundScene::create());
    return scene;
}

// @0064c96c
ResetWorkaroundScene::ResetWorkaroundScene()
{
    // _frameCount is left uninitialised (reset in init()).
}

// @0064ca6c (D1), @0064ca80 (D0)
ResetWorkaroundScene::~ResetWorkaroundScene()
{
}

// @0064c99c
bool ResetWorkaroundScene::init()
{
    // Layer::init() is not called.
    _frameCount = 0;
    scheduleUpdate();
    return true;
}

// @0064c9b8
void ResetWorkaroundScene::update(float dt)
{
    // Give the old Gameplay scene a few frames to tear down before building a fresh one.
    if (++_frameCount == 5)
    {
        unscheduleUpdate();
        Scene* scene = Gameplay::createScene("", nullptr);
        Director::getInstance()->replaceScene(scene);
    }
}
