#pragma once

#include "cocos2d.h"

#include <string>

// Sprite of one character portrait on the main menu's perspective carousel
// (PerspectiveCharacters): keeps the carousel depth (z), the base scale and the character index.
//
// arm64 sizeof 0x540; own fields 0x530..0x540 (cocos2d::Sprite dsize 0x52d). The secondary vtable
// (TextureProtocol at +0x2f8) gets compiler-generated non-virtual destructor thunks
// (@005a5258 D1, @005a5284 D0).
class CharacterSprite : public cocos2d::Sprite
{
public:
    CharacterSprite();                                      // @005a5208
    ~CharacterSprite() override;                            // @005a5254 (D1), @005a5260 (D0)

    // new CharacterSprite() (plain, throwing new) + initWithFile(filename) + autorelease;
    // deleted on failure.
    static CharacterSprite* create(const std::string& filename);  // @005a52ac

    float getZ();                                           // @005a5328
    void setZ(float z);                                     // @005a5330
    int getCharacterIndex();                                // @005a5338
    void setCharacterIndex(int characterIndex);             // @005a5340
    float getScaleFactor();                                 // @005a5348
    void setScaleFactor(float scaleFactor);                 // @005a5350

protected:
    // RE-TODO(@005a5208): initialised to 0 by the constructor, never read or written otherwise.
    float _unk0x530 = 0.0f;         // +0x530
    float _scaleFactor = 1.0f;      // +0x534
    int _characterIndex = -1;       // +0x538  -1 = generic portrait
    float _z = 0.0f;                // +0x53c  carousel depth
};
