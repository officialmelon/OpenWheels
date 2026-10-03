#pragma once

#include "Special.h"

// iOS TokenRef : Special (no ivars) - editor reference for the token (coin), level item 31
// (-> Token).
//
// Sprite: "e_coin.png"; the create hook sets refRect to (0, 0, texture w, h).
// canDragModify 0, canRotate 1, but setRotation is a no-op. Shape count 1.
// propertyKeys: Special's defaults -> p0 xMeters, p1 yMeters, p2 angle, p3 fixed, p4 sleeping
// (Token::init reads p0..p2).
// UI keys: xMeters, yMeters (no inputs from Special - iOS behaviour).
class TokenRef : public Special
{
public:
    CREATE_FUNC(TokenRef);

    bool init() override;                                                   // @ios 1000bccc8
    void setRotation(float rotation) override;  // no-op                       @ios 1000bcd48
    void onEnter() override;                                                // @ios 1000bcd4c
    std::vector<std::string> propertyKeysForUI() override;                  // @ios 1000bcda0
    void createRef() override;  // iOS -create                                 @ios 1000bce14
};
