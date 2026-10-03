#include "CharacterData.h"

USING_NS_CC;

// @005a1ff4
bool CharacterData::init()
{
    std::string path = FileUtils::getInstance()->fullPathForFilename("Characters.plist");
    _characterDict = FileUtils::getInstance()->getValueMapFromFile(path.c_str());
    return true;
}

// @005a221c
CharacterData::~CharacterData()
{
    _characterDict.clear();
}
