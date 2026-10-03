#pragma once

// LevelDataElement: a cocos2d::Ref wrapper around one tinyxml2 element of the level XML, with
// typed attribute accessors. sizeof 0x30 (Ref 0x28 + one pointer).
// No user-declared destructor: D1 is Ref::~Ref, and the deleting destructor / vtable are emitted
// as COMDAT in the TUs that construct it (first one: LevelB2D.cpp).
// Besides create() (autoreleased), LevelB2D also does `new LevelDataElement(); init(el); ... delete`.

#include <string>

#include "base/CCRef.h"

namespace tinyxml2 {
class XMLElement;
}

class LevelDataElement : public cocos2d::Ref
{
public:
    static LevelDataElement* create(tinyxml2::XMLElement* data);
    bool init(tinyxml2::XMLElement* data);

    // Each returns true when the attribute exists (and, for float/int, parses: XML_SUCCESS).
    bool floatAttribute(std::string name, float* value);
    bool intAttribute(std::string name, int* value);
    // "t" or "1" => true, any other value => false; *value untouched when the attribute is absent.
    bool boolAttribute(std::string name, bool* value);

    int getType();  // attribute "t", -1 when absent/unparsable
    void setData(tinyxml2::XMLElement* data);
    const char* stringAttribute(std::string name);  // nullptr when absent
    tinyxml2::XMLElement* getData();

private:
    tinyxml2::XMLElement* _data;  // +0x28
};
