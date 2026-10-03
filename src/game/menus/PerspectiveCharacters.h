#pragma once

#include "cocos2d.h"

#include <cstdint>
#include <vector>

class CharacterSprite;

// The row of character portraits standing in depth behind the main menu / level select. One
// CharacterSprite per chapter (character id -1 = generic portrait), tagged with its index and
// projected every frame with a simple perspective: scale = scaleFactor / (1 - z / _zc). The
// portrait at _index is swapped for its full-resolution version once the row has
// settled on it.
//
// arm64 sizeof 0x360 (create() does a plain `new`, 0x360 bytes).
class PerspectiveCharacters : public cocos2d::Node
{
public:
    void onEnter() override;                           // @006023d4  just Node::onEnter()
    PerspectiveCharacters();                           // @006023d8
    ~PerspectiveCharacters() override;                 // @00602420 (D1), @00602460 (D0)

    // Plain new + initWithIDs + autorelease (no nothrow, no failure path).
    static PerspectiveCharacters* create(std::vector<int> characterIds, int characterIndex,
                                         bool isMainMenu);                                // @00602484
    bool initWithIDs(std::vector<int> characterIds, int characterIndex, bool isMainMenu);  // @0060262c

    void setIsMainMenu(bool isMainMenu);               // @00602950
    bool isMainMenu();                                 // @00602958
    float getCharacterIndex();                         // @00602960
    void setCharacterIndex(float characterIndex);      // @00602968
    CharacterSprite* getForemostSprite();              // @00602970
    void setOffsetXTarget(float offsetXTarget);        // @00602978
    // Same field as setCharacterIndex.
    void setIndex(float index);                        // @00602980
    void update(float dt) override;                    // @00602988  vptr+0x3d8

    // Tuning hooks for MainMenu::sliderEvent (debug sliders, value in [0,1]).
    void setZC(float value);                           // @00602df8  _zc = (v*2-1)*2000
    void setOffsetX(float value);                      // @00602e18  _offsetXTarget = v*3000
    void setEyeX(float value);                         // @00602e30  _eyeX = (v*2-1)*5000
    void setSpaceZ(float value);                       // @00602e54  _spaceZDefault = (v*2-1)*2790

protected:
    // Member names from the iOS PerspectiveCharactersLayer ivars (same order; the Android port adds
    // _visibleOrigin and drops the UISlider members).
    bool _isMainMenu;                                  // +0x2f8
    std::vector<int> _characterIDs;                    // +0x300
    cocos2d::Vec2 _visibleOrigin;                      // +0x318  Director visible origin at init
    float _index;                                      // +0x320  target index (fractional while scrolling)
    // Zeroed by the constructor and never used again on Android.
    float _targetZC;                                   // +0x324
    float _targetOffsetX;                              // +0x328
    float _targetEyeX;                                 // +0x32c
    float _targetSpaceZ;                               // +0x330
    float _zc;                                         // +0x334  -1280; projection: 1 - z / _zc
    float _offsetX;                                    // +0x338  eases 1/10 per frame towards _offsetXTarget
    float _offsetXTarget;                              // +0x33c  2430
    float _eyeX;                                       // +0x340  -2600
    float _initialZ;                                   // +0x344  z of portrait 0; eases 1/5 per frame towards -_index * _spaceZDefault
    float _spaceZDefault;                              // +0x348  2790; depth per index used for the target (setSpaceZ)
    float _spaceZ;                                     // +0x34c  2790; depth step between portraits
    CharacterSprite* _foremostSprite;                  // +0x350  portrait at _index
    // RE-TODO(@00602484): 8 bytes at +0x358 are never accessed; kept so arm64 sizeof stays 0x360.
    uint8_t _unk358[8];                                // +0x358
};
