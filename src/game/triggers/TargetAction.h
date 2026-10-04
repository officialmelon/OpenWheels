#pragma once

// TargetAction: trigger action on a level shape (ShapeItem / fixture): wake, make static or
// dynamic, fade, impulse, release/delete, change collision. sizeof 0xe8 (arm64).

#include <vector>

#include "TargetActionBase.h"

namespace cocos2d {
class Sprite;
}

class ShapeItem;

class TargetAction : public TargetActionBase
{
public:
    TargetAction();
    virtual ~TargetAction();

    static TargetAction* create(cocos2d::Sprite* refSprite, b2Fixture* fixture,
                                cocos2d::Sprite* sprite, int action,
                                std::vector<float> properties);
    bool initWithRefSprite(cocos2d::Sprite* refSprite, b2Fixture* fixture,
                           cocos2d::Sprite* sprite, int action,
                           std::vector<float> properties);
    static TargetAction* create(ShapeItem* shapeItem, b2Fixture* fixture,
                                cocos2d::Sprite* sprite, int action,
                                std::vector<float> properties);
    bool initWithShapeItem(ShapeItem* shapeItem, b2Fixture* fixture,
                           cocos2d::Sprite* sprite, int action,
                           std::vector<float> properties);
    void updateTargetActionsForCurrentShape(b2Fixture* oldFixture, b2Fixture* newFixture);
    void updateTargetActionsForShapeItem(ShapeItem* shapeItem, b2Fixture* oldFixture,
                                         b2Fixture* newFixture);

    // _action: 0 wake, 1 make static, 2 make dynamic, 3 fade (actions()), 4 impulse,
    // 5 release from group/body, 6 delete, 7 change collision.
    virtual void singleAction() override;
    virtual void actions() override;

    // ONLINE (PC addition): Flash TargetAction.singleAction for converted browser levels.
    void onlineSingleAction();

protected:
    // Names from the iOS original's TargetAction ivars (iOS `_action` was an NSString).
    b2Fixture* _shape;                // +0xa0
    cocos2d::Sprite* _refSprite;      // +0xa8 (null from LevelB2D)
    ShapeItem* _shapeItem;            // +0xb0
    cocos2d::Sprite* _sprite;         // +0xb8 (null from LevelB2D)
    int _actionIndex;                 // +0xc0
    int _action;                      // +0xc4 = actionIndex, set by the ShapeItem path only (sic)
    std::vector<float> _properties;   // +0xc8
    float _lastAngle;                 // +0xe0 -refSprite->getRotation() in radians
    float _counter;                   // +0xe4
};
