#pragma once
// ONLINE (PC addition): browser special 31, Token (Flash userspecials/Token + editor TokenRef,
// HUD from UserLevel.createTokenHUD / tokenFound). The Android Token is a stub; this replaces it
// in converted browser levels (Override).
//
// XML (TokenRef._attributes): p0 x, p1 y, p2 tokenType 1..6.
//
// A spinning coin (CoinMC, face = token type) with a static sensor circle (r 23 px). The first
// living character head/chest/pelvis touching it collects it (Bleep sound): the coin goes, the
// HUD's "found/total" counter (top right, TokenIcon) goes up, and collecting the last token
// completes the level.

#include "online/items/FlashSpecials.h"
#include "online/items/MiscItemUtil.h"

namespace cocos2d {
class Sprite;
}

namespace online {

class Token : public FlashItem
{
public:
    ~Token() override;
    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override;
    void beginContact(b2Fixture* fixture, b2Fixture* otherFixture, b2Contact* contact) override;
    void singleAction() override;
    void paint() override;

private:
    cocos2d::Node* _mc = nullptr;  // retained
    cocos2d::Sprite* _edge = nullptr;
    cocos2d::Node* _face = nullptr;
    b2Body* _body = nullptr;
    b2Fixture* _shape = nullptr;
    bool _collected = false;
    int _frame = 0;  // CoinMC frame - 1 (plays 1..16 at 30 fps)
    misc::FlashClock _clock;
};

}  // namespace online
