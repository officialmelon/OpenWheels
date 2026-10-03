#include "LevelDataElement.h"

#include <cstring>
#include <new>

#include "tinyxml2/tinyxml2.h"

// ~LevelDataElement() @005dc258: implicit (no user-declared destructor); the deleting destructor
// and the vtable are COMDAT, emitted in the first TU that constructs the class (LevelB2D.cpp).

// @005dcb20
LevelDataElement* LevelDataElement::create(tinyxml2::XMLElement* data)
{
    // No value-initialisation in the original (no zero fill before Ref::Ref()).
    LevelDataElement* element = new (std::nothrow) LevelDataElement;
    if (element && element->init(data))
    {
        element->autorelease();
        return element;
    }
    CC_SAFE_DELETE(element);
    return nullptr;
}

// @005dcb98
bool LevelDataElement::init(tinyxml2::XMLElement* data)
{
    _data = data;
    return true;
}

// @005dcba8
bool LevelDataElement::floatAttribute(std::string name, float* value)
{
    return _data->QueryFloatAttribute(name.c_str(), value) == tinyxml2::XML_SUCCESS;
}

// @005dcbf0
bool LevelDataElement::intAttribute(std::string name, int* value)
{
    return _data->QueryIntAttribute(name.c_str(), value) == tinyxml2::XML_SUCCESS;
}

// @005dcc38
bool LevelDataElement::boolAttribute(std::string name, bool* value)
{
    const char* attribute = _data->Attribute(name.c_str());
    if (attribute)
    {
        *value = strcmp(attribute, "t") == 0 || strcmp(attribute, "1") == 0;
    }
    return attribute != nullptr;
}

// @005dccb8
int LevelDataElement::getType()
{
    int type;
    if (_data->QueryIntAttribute("t", &type) != tinyxml2::XML_SUCCESS)
    {
        type = -1;
    }
    return type;
}

// @005dcd28
void LevelDataElement::setData(tinyxml2::XMLElement* data)
{
    _data = data;
}

// @005dcd30
const char* LevelDataElement::stringAttribute(std::string name)
{
    return _data->Attribute(name.c_str());
}

// @005dcd4c
tinyxml2::XMLElement* LevelDataElement::getData()
{
    return _data;
}
