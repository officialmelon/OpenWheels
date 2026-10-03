#pragma once

// TargetActionBase: common base of the actions a Trigger fires at its targets.
// No out-of-line code in the binary: constructor/destructor are inlined into the subclasses
// and it adds no virtuals (all TargetAction* vtables are LevelItem's 36 words).
// Its two fields live in LevelItem's tail padding (+0x94, +0x98); sizeof 0xa0.

#include "LevelItem.h"

// TargetActionBase::_targetActionType. Trigger::singleAction dispatches on it.
enum TargetActionBaseType
{
    TargetActionBaseTypeShape = 0,      // TargetAction (shape / ShapeItem)
    TargetActionBaseTypeSpecial = 1,    // TargetActionSpecial (LevelItem trigger hooks)
    TargetActionBaseTypeGroup = 2,      // TargetActionGroup
    TargetActionBaseTypeRevJoint = 3,   // TargetActionRevJoint
    TargetActionBaseTypePrisJoint = 4,  // TargetActionPrisJoint
    TargetActionBaseTypeTrigger = 5,    // TargetActionTrigger (always fired immediately)
};

class TargetActionBase : public LevelItem
{
public:
    TargetActionBase() : _targetActionType(0), _instant(false) {}

    // Read directly by Trigger::singleAction.
    int _targetActionType;  // +0x94 TargetActionBaseType (Android-only; name ours)
    bool _instant;          // +0x98 (iOS name) true: fire singleAction() once;
                            //       false: level->addToActions(), actions() runs every step
};
