#include "SoccerBallRef.h"

USING_NS_CC;

// @ios 10004d388
bool SoccerBallRef::init()
{
    if (!Special::initWithSpriteFrameName("e_soccerball.png"))
    {
        return false;
    }
    setCanDragModify(false);
    setCanRotate(false);
    setLevelItemID(10);
    return true;
}

// @ios 10004d3fc
std::vector<std::string> SoccerBallRef::propertyKeysForUI()
{
    return {"xMeters", "yMeters", "angle"};
}
