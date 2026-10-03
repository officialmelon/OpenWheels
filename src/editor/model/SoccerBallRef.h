#pragma once

#include "Special.h"

// iOS SoccerBallRef : Special (no ivars) - editor reference for the soccer ball, level item 10
// (-> SoccerBall).
//
// Sprite: "e_soccerball.png". canDragModify 0, canRotate 0. No shape count.
// propertyKeys: Special's defaults -> p0 xMeters, p1 yMeters, p2 angle, p3 fixed, p4 sleeping
// (SoccerBall::init reads p0, p1 only).
// UI keys: xMeters, yMeters, angle (xMeters/yMeters get no input from Special - iOS behaviour).
class SoccerBallRef : public Special
{
public:
    CREATE_FUNC(SoccerBallRef);

    bool init() override;                                                   // @ios 10004d388
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 10004d3fc
};
