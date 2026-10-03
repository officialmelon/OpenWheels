#pragma once

#include "cocos2d.h"

class GLESDebugDraw;
class b2World;

// Sprite that renders the Box2D world's debug draw (shapes + joints) on top of the level; created by
// Session when debug drawing is switched on. arm64 sizeof 0x590.
// Own fields start at 0x530 (cocos2d::Sprite nvsize 0x52d).
class B2DebugDrawLayer : public cocos2d::Sprite
{
public:
    // Note: this constructor leaves _world uninitialised; the b2World* one leaves _debugDraw
    // uninitialised (both as in the original).
    B2DebugDrawLayer();                                                        // @00581dc4
    // Deletes _debugDraw.
    ~B2DebugDrawLayer() override;                                              // @00581e1c (D1), @00581e7c (D0)

    // new B2DebugDrawLayer(world) (plain new) + init(ratio) inlined + autorelease.
    static B2DebugDrawLayer* create(b2World* world, float ratio);              // @00581ec8
    // _debugDraw = new GLESDebugDraw(ratio); world->SetDebugDraw(_debugDraw);
    // flags e_shapeBit | e_jointBit (3). Returns true.
    bool init(float ratio);                                                    // @00581f68
    B2DebugDrawLayer(b2World* world);                                          // @00581fd8

    // Queues _customCommand (globalZOrder, transform, flags) with onDraw bound to transform/flags.
    void draw(cocos2d::Renderer* renderer, const cocos2d::Mat4& transform, uint32_t flags) override; // @00582034
    // Modelview stack push/load(transform), enable position attrib, _world->DrawDebugData(), pop.
    void onDraw(const cocos2d::Mat4& transform, uint32_t flags);               // @00582120

protected:
    b2World* _world;                             // +0x530
    GLESDebugDraw* _debugDraw;                   // +0x538 owned
    cocos2d::CustomCommand _customCommand;       // +0x540 (sizeof 0x50, 16-byte aligned)
};
