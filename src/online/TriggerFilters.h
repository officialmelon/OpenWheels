#pragma once
// ONLINE (PC addition): Flash trigger-action helpers for converted browser levels
// (level/TargetAction.as, TargetActionGroup.as). Only called while online::flashLevel().

#include <functional>

#include "Box2D/Box2D.h"

namespace online {

// Filter of a shape that Flash rebuilds as fixed ("set to fixed"): TargetAction case 1. With
// groupStyle (TargetActionGroup "set to fixed") levels <= 1.84 keep the filter unchanged.
void filterToFixed(b2Filter* filter, float levelVersion, bool groupStyle = false);
// Inverse, "set to non fixed" (TargetAction case 2 / TargetActionGroup).
void filterToNonFixed(b2Filter* filter, float levelVersion, bool groupStyle = false);
// "change collision" (TargetAction case 7 / TargetActionGroup): collision 1..7, fixed = the body
// has no mass. *sensor receives Flash's m_isSensor (only "no collision" before 1.82).
void filterForCollision(b2Filter* filter, bool* sensor, int collision, bool fixed,
                        float levelVersion);

// Destroys the joints attached to body (all, or those accepted by filter), telling the session's
// DestructionListener first as b2World::DestroyBody would (Flash destroys a body's joints when it
// rebuilds the shape).
void destroyJointsOf(b2Body* body, const std::function<bool(b2Joint*)>& filter = nullptr);

}  // namespace online
