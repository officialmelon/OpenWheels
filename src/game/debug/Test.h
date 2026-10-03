#pragma once

#include "cocos2d.h"

// Developer test layer: shows one character sprite at (100, 100). Not reachable from the shipped
// menus. No own fields (arm64 sizeof 0x320).
class Test : public cocos2d::Layer
{
public:
    Test();                                                                    // @00636afc
    ~Test() override;                                                          // @00636b2c (D1), @00636b30 (D0)

    // CREATE_FUNC body (new (std::nothrow), init(), autorelease), emitted out of line.
    static Test* create();                                                     // @00636b54
    // Scene::create() + the create() body inlined + addChild.
    static cocos2d::Scene* createScene();                                      // @00636be0
    // Does not call Layer::init(). Returns true.
    bool init() override;                                                      // @00636c88
};
