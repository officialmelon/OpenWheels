#pragma once

// BackgroundLayer: the level background (gradient fills, tiling sprites and parallax Backdrops).
// cocos2d::Node subclass, sizeof 0x330 (Node is 16-byte aligned). Only the destructor is virtual
// (update(Vec2) and init(...) are new, non-virtual overloads).
// The Session creates it; LevelB2D::addInfo calls init() once the level's <info> is known.

#include <string>
#include <vector>

#include "2d/CCNode.h"
#include "base/ccTypes.h"
#include "math/CCGeometry.h"
#include "math/Vec2.h"

namespace cocos2d {
class LayerGradient;
class Sprite;
}

class Backdrop;
class LevelDataElement;

// Level <info bg="..."> value. Enumerator names are descriptive (only the type name is original);
// values and art are those of BackgroundLayer::create(LevelDataElement*).
enum backgrounds
{
    BackgroundNone = 0,               // plain colour (info "bgc") via LayerColor in init
    BackgroundGreenHills = 1,         // blueGradient + greenhills batch (greenSource1..3)
    BackgroundCity = 2,               // cityGradient
    BackgroundLab = 3,                // lab_tile
    BackgroundBricks = 4,             // bricks_256_blur1
    BackgroundNightHorizon = 4000,    // nightHorizon gradient
    BackgroundGreenHillsSimple = 4001,// blueGradient + greenhills batch (greenSource3 only)
    BackgroundSunset = 4002,          // sunsetGradient + sunset batch (sun, mountain1/2)
    BackgroundDots = 4003,            // dots_tile
};

class BackgroundLayer : public cocos2d::Node
{
public:
    static BackgroundLayer* create();
    static BackgroundLayer* create(cocos2d::Size stageSize, backgrounds type, long color,
                                   float ptmRatio, LevelDataElement* parameters);
    bool init(cocos2d::Size stageSize, backgrounds type, long color, float ptmRatio,
              LevelDataElement* parameters);

    BackgroundLayer();
    virtual ~BackgroundLayer();  // deletes the Backdrops

    // Builds the art for _type (not a factory despite the name).
    void create(LevelDataElement* parameters);
    cocos2d::Sprite* addFillBGWithFile(std::string file);
    void addSpriteBatchNode(std::string name);  // loads "<name>.plist", _batchNode = Node::create()
    // Does not use `this` (callers pass a stale x0) but is not static in the original.
    cocos2d::LayerGradient* createGradient(LevelDataElement* parameters, float endGradientAlpha);
    cocos2d::Sprite* createTilingSprite(std::string file, float scale);
    cocos2d::Sprite* spriteWithTextureFile(std::string file, cocos2d::Color4F color,
                                           cocos2d::Size size, float endGradientAlpha);
    void update(cocos2d::Vec2 pos);  // Backdrop::update on every backdrop

private:
    std::vector<Backdrop*> _backdrops;  // +0x2f8
    cocos2d::Node* _batchNode;          // +0x310  container for the sprite-frame backdrops
    backgrounds _type;                  // +0x318
    cocos2d::Size _stageSize;           // +0x31c
    float _ptmRatio;                    // +0x324  ctor 1.0
};
