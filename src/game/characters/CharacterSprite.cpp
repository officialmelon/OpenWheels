#include "CharacterSprite.h"

USING_NS_CC;

// @005a5208
CharacterSprite::CharacterSprite()
{
}

// @005a5254
CharacterSprite::~CharacterSprite()
{
}

// @005a52ac
CharacterSprite* CharacterSprite::create(const std::string& filename)
{
    CharacterSprite* sprite = new CharacterSprite();
    if (sprite->initWithFile(filename))
    {
        sprite->autorelease();
        return sprite;
    }
    delete sprite;
    return nullptr;
}

// @005a5328
float CharacterSprite::getZ()
{
    return _z;
}

// @005a5330
void CharacterSprite::setZ(float z)
{
    _z = z;
}

// @005a5338
int CharacterSprite::getCharacterIndex()
{
    return _characterIndex;
}

// @005a5340
void CharacterSprite::setCharacterIndex(int characterIndex)
{
    _characterIndex = characterIndex;
}

// @005a5348
float CharacterSprite::getScaleFactor()
{
    return _scaleFactor;
}

// @005a5350
void CharacterSprite::setScaleFactor(float scaleFactor)
{
    _scaleFactor = scaleFactor;
}
