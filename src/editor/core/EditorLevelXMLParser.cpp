#include "EditorLevelXMLParser.h"

#include <cstdlib>
#include <cstring>

#include "cocos2d.h"
#include "tinyxml2/tinyxml2.h"

USING_NS_CC;

namespace {

// -[NSString floatValue] on a TBXML attribute (leading float, 0 when unparsable).
float nsFloatValue(const char* s)
{
    return s ? (float)std::strtod(s, nullptr) : 0.0f;
}

// -[NSString intValue] (leading integer, saturating like NSScanner; 0 when unparsable).
int nsIntValue(const char* s)
{
    if (!s)
    {
        return 0;
    }
    long long v = std::strtoll(s, nullptr, 10);
    if (v > INT_MAX)
    {
        return INT_MAX;
    }
    if (v < INT_MIN)
    {
        return INT_MIN;
    }
    return (int)v;
}

// -[NSString boolValue]: skips whitespace, then YES for Y/y/T/t or a non-zero digit after an
// optional sign and leading zeros.
bool nsBoolValue(const char* s)
{
    if (!s)
    {
        return false;
    }
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
    {
        ++s;
    }
    if (*s == 'Y' || *s == 'y' || *s == 'T' || *s == 't')
    {
        return true;
    }
    if (*s == '+' || *s == '-')
    {
        ++s;
    }
    while (*s == '0')
    {
        ++s;
    }
    return *s >= '1' && *s <= '9';
}

// The raw attribute as a dictionary value. Port addition: Flash-exported (shipped) levels write
// booleans as "t"/"f", which -floatValue would read as 0; the iOS editor never loads such files
// (its own output uses 0/1), so they are mapped to 1/0 to make shipped levels editable.
Value attributeValue(const char* s)
{
    if (s[0] == 't' && s[1] == '\0')
    {
        return Value(1.0f);
    }
    if (s[0] == 'f' && s[1] == '\0')
    {
        return Value(0.0f);
    }
    return Value(std::string(s));
}

float dictFloat(const ValueMap& dict, const char* key)
{
    auto it = dict.find(key);
    if (it == dict.end())
    {
        return 0.0f;
    }
    if (it->second.getType() == Value::Type::STRING)
    {
        return nsFloatValue(it->second.asString().c_str());
    }
    return it->second.asFloat();
}

}  // namespace

// @ios 100108f10
EditorLevelXMLParser::EditorLevelXMLParser(const std::string& xml, EditorLevelXMLParserDelegate* delegate)
    : _delegate(delegate), _tbxml(new tinyxml2::XMLDocument())
{
    if (_tbxml->Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS)
    {
        _tbxml->DeleteChildren();
    }
}

// @ios 100108fa0
EditorLevelXMLParser::~EditorLevelXMLParser()
{
    delete _tbxml;
}

// @ios 100108ff8
bool EditorLevelXMLParser::parse()
{
    if (!_tbxml->RootElement())
    {
        return false;
    }
    parseLevelInfo();
    parseLevelObjects();
    return true;
}

// @ios 10010901c
void EditorLevelXMLParser::parseLevelInfo()
{
    tinyxml2::XMLElement* root = _tbxml->RootElement();
    if (!root)
    {
        return;
    }
    tinyxml2::XMLElement* info = root->FirstChildElement("info");
    if (!info)
    {
        // RE-TODO(@10010901c): TBXML returns nil attributes for a nil element; iOS would then
        // throw in setObject:forKey:. The port skips the <info> callbacks.
        return;
    }
    _ptmRatio = nsFloatValue(info->Attribute("ptm"));
    _stageWidth = (double)nsFloatValue(info->Attribute("sw"));
    _stageHeight = (double)nsFloatValue(info->Attribute("sh"));
    _registration = (short)nsIntValue(info->Attribute("r"));
    _clockwise = nsBoolValue(info->Attribute("cw"));

    if (_delegate)
    {
        _version = nsFloatValue(info->Attribute("v"));
        int background = nsIntValue(info->Attribute("bg"));
        int color = nsIntValue(info->Attribute("bgc"));
        ValueVector parameters;
        const char* parameter = info->Attribute("b0");
        int index = 0;
        while (parameter)
        {
            parameters.push_back(Value(std::string(parameter)));
            ++index;
            std::string key = StringUtils::format("b%i", index);
            parameter = info->Attribute(key.c_str());
        }
        _delegate->setVersion(_version, (unsigned int)background, (unsigned int)color, parameters);

        ValueMap character;
        for (const char* key : {"x", "y", "c", "f", "h"})
        {
            const char* value = info->Attribute(key);
            if (value)
            {
                character[key] = Value(std::string(value));
            }
        }
        if (_ptmRatio != 0.0f)
        {
            float x = dictFloat(character, "x");
            float y = dictFloat(character, "y");
            if (_registration == 1)
            {
                y = (float)(_stageHeight - (double)y);
            }
            character["x"] = Value(x / _ptmRatio);
            character["y"] = Value(y / _ptmRatio);
        }
        _delegate->addCharacter(character);
    }
}

// @ios 100109450
void EditorLevelXMLParser::parseLevelObjects()
{
    tinyxml2::XMLElement* root = _tbxml->RootElement();
    addShapesWithElement(root->FirstChildElement("shapes"));
    addSpecialsWithElement(root->FirstChildElement("specials"));
    // groups, joints, triggers: the editor (the only client) implements none of their delegate
    // methods (respondsToSelector: NO), so nothing is parsed for them.
}

// @ios 100109550
void EditorLevelXMLParser::addSpecialsWithElement(tinyxml2::XMLElement* specials)
{
    if (!specials || !_delegate)
    {
        return;
    }
    unsigned int index = 0;
    for (tinyxml2::XMLElement* special = specials->FirstChildElement("sp"); special;
         special = special->NextSiblingElement())
    {
        _delegate->addSpecial(specialDictFromTBXMLElement(special), index);
        ++index;
    }
}

// @ios 1001095dc
void EditorLevelXMLParser::addShapesWithElement(tinyxml2::XMLElement* shapes)
{
    if (!shapes || !_delegate)
    {
        return;
    }
    unsigned int index = 0;
    for (tinyxml2::XMLElement* shape = shapes->FirstChildElement("sh"); shape;
         shape = shape->NextSiblingElement())
    {
        _delegate->addShape(shapeDictFromTBXMLElement(shape), index);
        ++index;
    }
}

// @ios 100109960
ValueMap EditorLevelXMLParser::shapeDictFromTBXMLElement(tinyxml2::XMLElement* shape)
{
    ValueMap dict;
    const char* t = shape->Attribute("t");
    int type = nsIntValue(t);
    if (t)
    {
        dict["t"] = Value(std::string(t));
    }
    const char* interactive = shape->Attribute("i");
    if (interactive)
    {
        dict["i"] = attributeValue(interactive);
    }
    const char* p = shape->Attribute("p0");
    int index = 0;
    while (p)
    {
        dict[StringUtils::format("p%i", index)] = attributeValue(p);
        ++index;
        p = shape->Attribute(StringUtils::format("p%i", index).c_str());
    }
    if (_ptmRatio == 0.0f)
    {
        return dict;
    }
    auto flip = [this](float y) {
        return _registration == 1 ? (float)(_stageHeight - (double)y) : y;
    };
    if (type == 5 || type == 4 || type == 3)
    {
        // Art (5) / polygon (4) / type 3: position divided by ptm (type 4/3 also p2/p3), plus
        // vertex data under <v> that the editor cannot represent (not built by the port).
        float p0 = dictFloat(dict, "p0");
        float p1 = flip(dictFloat(dict, "p1"));
        dict["p0"] = Value(p0 / _ptmRatio);
        dict["p1"] = Value(p1 / _ptmRatio);
        if (type != 5)
        {
            float p2 = dictFloat(dict, "p2");
            float p3 = dictFloat(dict, "p3");
            dict["p2"] = Value(p2 / _ptmRatio);
            dict["p3"] = Value(p3 / _ptmRatio);
        }
        return dict;
    }
    // Rectangle / circle / triangle: p0..p3 (x, y, width, height) * (1 / ptm).
    float p0 = dictFloat(dict, "p0");
    float p1 = flip(dictFloat(dict, "p1"));
    float p2 = dictFloat(dict, "p2");
    float p3 = dictFloat(dict, "p3");
    float inv = 1.0f / _ptmRatio;
    dict["p0"] = Value(p0 * inv);
    dict["p1"] = Value(p1 * inv);
    dict["p2"] = Value(p2 * inv);
    dict["p3"] = Value(p3 * inv);
    return dict;
}

// @ios 10010a5f4
ValueMap EditorLevelXMLParser::specialDictFromTBXMLElement(tinyxml2::XMLElement* special)
{
    ValueMap dict;
    const char* t = special->Attribute("t");
    if (t)
    {
        dict["t"] = Value(std::string(t));
    }
    int type = nsIntValue(t);
    const char* p = special->Attribute("p0");
    int index = 0;
    while (p)
    {
        dict[StringUtils::format("p%i", index)] = attributeValue(p);
        ++index;
        p = special->Attribute(StringUtils::format("p%i", index).c_str());
    }
    if (_ptmRatio == 0.0f || (unsigned int)type >= 0x23)
    {
        return dict;
    }
    auto flip = [this](float y) {
        return _registration == 1 ? (float)(_stageHeight - (double)y) : y;
    };
    unsigned long long bit = 1ULL << (type & 0x3f);
    if (bit & 0x472909765ULL)
    {
        float p0 = dictFloat(dict, "p0");
        float p1 = flip(dictFloat(dict, "p1"));
        dict["p0"] = Value(p0 / _ptmRatio);
        dict["p1"] = Value(p1 / _ptmRatio);
    }
    else if (bit & 0x18ULL)
    {
        float p0 = dictFloat(dict, "p0");
        float p1 = flip(dictFloat(dict, "p1"));
        float p2 = dictFloat(dict, "p2");
        float p3 = dictFloat(dict, "p3");
        float inv = 1.0f / _ptmRatio;
        dict["p0"] = Value(p0 * inv);
        dict["p1"] = Value(p1 * inv);
        dict["p2"] = Value(p2 * inv);
        dict["p3"] = Value(p3 * inv);
    }
    else if (type == 7)
    {
        float p0 = dictFloat(dict, "p0");
        float p1 = flip(dictFloat(dict, "p1"));
        float p2 = dictFloat(dict, "p2");
        float inv = 1.0f / _ptmRatio;
        dict["p0"] = Value(p0 * inv);
        dict["p1"] = Value(p1 * inv);
        dict["p2"] = Value(p2 * inv);
    }
    return dict;
}
