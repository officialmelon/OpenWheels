#pragma once

#include "cocos2d.h"

// Holds the contents of "Characters.plist". Nothing in the Android build creates it (no
// constructor, create() or caller of init() exists in the binary).
//
// arm64 sizeof 0x50; own field at 0x28 (cocos2d::Ref dsize 0x21).
class CharacterData : public cocos2d::Ref
{
public:
    // Clears _characterDict (then the member is destroyed as usual).
    ~CharacterData() override;                              // @005a221c (D2), @005a2304 (D0)

    // _characterDict = FileUtils::getValueMapFromFile(fullPathForFilename("Characters.plist")).
    // Always returns true.
    bool init();                                            // @005a1ff4

protected:
    cocos2d::ValueMap _characterDict;                       // +0x28
};
