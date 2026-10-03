#pragma once

#include "Special.h"

class InputObject;

// iOS SlowMotionPanelRef : Special (instanceSize 0x280) - editor reference for the slow-motion
// panel, level item 5001 (-> SlowMotionPanel).
//
// Sprite: "e_1x1.png"; the create hook adds two mirrored "e_slowmotionpanel.png" (anchor (1, 0.5),
// scaleX +-1.01) and updateRefRect sizes refRect to panelWidth x panelHeight meters (centred).
// EditorSpriteBatchNode's update draws the panel outline through updateDrawingWithNode.
// Shape count 1.
// propertyKeys -> XML: p0 xMeters, p1 yMeters, p2 angle, p3 duration, p4 panelWidthMeters,
// p5 panelHeightMeters. NOTE: the Android SlowMotionPanel::init is a stub that reads p0..p2
// into locals only, so these panels have no effect in the reconstructed game.
// Defaults: on 0, duration 5, panelWidthMeters 2, panelHeightMeters 2.
// UI keys: x, y, angle, duration, panelWidthMeters, panelHeightMeters.
class SlowMotionPanelRef : public Special
{
public:
    CREATE_FUNC(SlowMotionPanelRef);

    bool init() override;                                                   // @ios 1000ded04
    void onEnter() override;                                                // @ios 1000dee50
    void setPanelWidthMeters(const cocos2d::Value& panelWidthMeters);  // + updateRefRect  @ios 1000deea4
    float panelWidthMeters();                                               // @ios 1000deed8
    void setPanelHeightMeters(const cocos2d::Value& panelHeightMeters); // + updateRefRect @ios 1000deee8
    float panelHeightMeters();                                              // @ios 1000def1c
    void updateRefRect();                                                   // @ios 1000def2c
    void setDuration(const cocos2d::Value& duration);                       // @ios 1000def7c
    float duration();                                                       // @ios 1000defac
    // duration: SliderInputObject "DURATION", 1..10; panelWidthMeters: "PANEL WIDTH", 2..5;
    // panelHeightMeters: "PANEL HEIGHT", 2..5; all 0 segments; else Special's.
    InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                const cocos2d::Rect& rect) override;  // @ios 1000defbc
    void createRef() override;  // iOS -create                                 @ios 1000df1a0
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000df24c
    // Draws the rotated refRect outline (drawPolyWithVerts, border 0.25) into the editor's
    // draw node (E3's EditorSpriteBatchNode::drawNode, iOS CCDrawNode).
    void updateDrawingWithNode(cocos2d::DrawNode* node) override;           // @ios 1000df2ec

    // port: KVC (duration, panelWidthMeters, panelHeightMeters), chains to Special.
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    bool on = false;                       // +0x264
    float _duration = 0.0f;                // +0x268  iOS ivar "duration" (clashes with duration())
    float _panelWidthMeters = 0.0f;        // +0x26c
    float _panelHeightMeters = 0.0f;       // +0x270
    cocos2d::Sprite* mc = nullptr;         // +0x278
};
