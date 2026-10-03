#pragma once

// StageCamera: scrolls the session node so the focus body stays inside a moving border box,
// clamps to the stage limits, shakes on demand and listens for "system_trigger" events.
// cocos2d::Ref subclass, sizeof 0xc8 (arm64).

#include "base/CCRef.h"
#include "math/CCGeometry.h"
#include "math/Vec2.h"

class b2Body;

namespace cocos2d {
class DrawNode;
class EventListenerCustom;
class Node;
}

class StageCamera : public cocos2d::Ref
{
public:
    StageCamera();
    virtual ~StageCamera();

    static StageCamera* create(cocos2d::Node* stage, b2Body* focus, float ptmRatio);
    bool init(cocos2d::Node* stage, b2Body* focus, float ptmRatio);
    void reset();
    void setFocus(b2Body* focus);
    b2Body* getFocus();
    cocos2d::Vec2 getMidScreen();
    float getYParticleLimit();
    void handleSystemTrigger(void* data);  // *(int*)data: 6 lead right, 7 lead left, 8 shake, 9 stop
    void shakeStage();
    void endShakeStage();
    void setLimits(cocos2d::Size limits);
    void setBorders();
    void debugDraw();
    void center();

protected:
    // Names from the iOS original's StageCamera ivars (the port keeps their order).
    cocos2d::Node* _containerObj;          // +0x28 the Session node that gets moved
    b2Body* _focus;                        // +0x30
    float _ptmRatio;                       // +0x38
    float _scale;                          // +0x3c 1.0f
    float _yParticleLimit;                 // +0x40 |container y| after the last center()
    float _midXPoints;                     // +0x44 winWidth * 0.5 / scale
    float _midYPoints;                     // +0x48
    cocos2d::Vec2 _midScreen;              // +0x4c screen middle in meters
    float _moveIncrementPixels;            // +0x54 border easing step (x, and y downwards)
    float _upInc;                          // +0x58 border easing step (y upwards)
    float _leftLimitPixels;                // +0x5c container position limits (setLimits)
    float _rightLimitPixels;               // +0x60
    float _topLimitPixels;                 // +0x64
    float _bottomLimitPixels;              // +0x68
    float _xFastForwardLeftSide;           // +0x6c border targets (window fractions, init)
    float _xFastForwardRightSide;          // +0x70   focus vx > 3
    float _xFastBackwardsLeftSide;         // +0x74   focus vx < -3
    float _xFastBackwardsRightSide;        // +0x78
    float _xNormalLeftSide;                // +0x7c   |vx| <= 3, _leadRight
    float _xNormalRightSide;               // +0x80
    float _xNormalLeftSideLeadLeft;        // +0x84   |vx| <= 3, !_leadRight
    float _xNormalRightSideLeadLeft;       // +0x88
    float _yFastTopSide;                   // +0x8c   focus vy < -3
    float _yFastBottomSide;                // +0x90
    float _yNormalTopSide;                 // +0x94
    float _yNormalBottomSide;              // +0x98
    float _leftBorderPixels;               // +0x9c current borders (eased towards the targets)
    float _rightBorderPixels;              // +0xa0
    float _topBorderPixels;                // +0xa4
    float _bottomBorderPixels;             // +0xa8
    bool _leadRight;                       // +0xac system trigger 6 sets, 7 clears
    float _shakeAmount;                    // +0xb0 decreases by 1/60 per center()
    float _halfShakeRange;                 // +0xb4
    cocos2d::EventListenerCustom* _systemTriggerListener;  // +0xb8 (retained; Android-only)
    cocos2d::DrawNode* _drawNode;          // +0xc0 debugDraw()
};
