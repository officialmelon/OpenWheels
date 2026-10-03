#pragma once

// TargetActionTrigger: a trigger acting on another trigger (activate / disable / enable).
// Has no user-declared constructor: create() value-initialises it (zero fill, then
// LevelItem() and the inlined TargetActionBase()). sizeof 0xe0 (arm64).

#include <vector>

#include "TargetActionBase.h"

namespace cocos2d {
class Sprite;
}

class TargetActionTrigger : public TargetActionBase
{
public:
    static TargetActionTrigger* create(cocos2d::Sprite* refSprite, LevelItem* sourceTrigger,
                                       LevelItem* receivingTrigger, int targetAction,
                                       std::vector<float> properties);
    bool initWithRefSprite(cocos2d::Sprite* refSprite, LevelItem* sourceTrigger,
                           LevelItem* receivingTrigger, int targetAction,
                           std::vector<float> properties);

    // _targetAction: 0 receiver->activateByTrigger(), 1 setDisabled(true), 2 setDisabled(false).
    virtual void singleAction() override;
    virtual ~TargetActionTrigger();

protected:
    // Names from the iOS original's TargetActionTrigger ivars (iOS _targetAction: NSString).
    cocos2d::Sprite* _refSprite;     // +0xa0 (always null from LevelB2D)
    int _targetAction;               // +0xa8
    std::vector<float> _properties;  // +0xb0
    int _counter;                    // +0xc8 never used
    LevelItem* _sourceTrigger;       // +0xd0 the Trigger owning this action
    LevelItem* _receivingTrigger;    // +0xd8 a Trigger
};
