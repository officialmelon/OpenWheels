#pragma once

#include "cocos2d.h"

// Developer UI test layer (ui::ScrollView with a wrapped label, label wrapping, centring helper). Not
// reachable from the shipped menus. No own fields (arm64 sizeof 0x320).
class UITest : public cocos2d::Layer
{
public:
    UITest();                                                                  // @00637324
    ~UITest() override;                                                        // @00637354 (D2), @00637368 (D0)

    // Scene::create() + CREATE_FUNC(UITest) inlined + addChild.
    static cocos2d::Scene* createScene();                                      // @0063738c
    CREATE_FUNC(UITest);

    // Layer::init(); dark grey (50,50,50) LayerColor of the visible size; scrollableNodeExample().
    bool init() override;                                                      // @00637434
    void scrollableNodeExample();                                              // @006374dc
    // node->setPosition(visibleSize / 2).
    void centerNodeToScreen(cocos2d::Node* node);                              // @00637774
    void labelWrapExample();                                                   // @006377e8
};
