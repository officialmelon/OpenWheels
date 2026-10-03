#include "LevelXMLParser.h"

#include <cstring>

#include "LevelDataElement.h"
#include "LevelXMLParserDelegateProtocol.h"
#include "tinyxml2/tinyxml2.h"

// @005e5044
bool LevelXMLParser::init(std::string xml, LevelXMLParserDelegateProtocol* delegate)
{
    if (xml.length() == 0)
    {
        return false;
    }
    _delegate = delegate;

    std::string data = xml;
    tinyxml2::XMLDocument document;
    tinyxml2::XMLElement* root;
    if (document.Parse(data.c_str(), data.length()) != tinyxml2::XML_SUCCESS ||
        (root = document.FirstChildElement()) == nullptr)
    {
        return false;
    }

    // Top-level sections are handled in document order.
    for (tinyxml2::XMLElement* node = root->FirstChildElement(); node != nullptr;
         node = node->NextSiblingElement())
    {
        const char* name = node->Value();
        if (strcmp(name, "info") == 0)
        {
            handleInfoNode(node);
        }
        else if (strcmp(name, "shapes") == 0)
        {
            handleShapesNode(node);
        }
        else if (strcmp(name, "specials") == 0)
        {
            handleSpecialsNode(node);
        }
        else if (strcmp(name, "groups") == 0)
        {
            handleGroupsNode(node);
        }
        else if (strcmp(name, "joints") == 0)
        {
            handleJointsNode(node);
        }
        else if (strcmp(name, "triggers") == 0)
        {
            handleTriggersNode(node);
        }
    }
    return true;
}

// @005e5480
void LevelXMLParser::handleInfoNode(tinyxml2::XMLElement* infoElement)
{
    if (_delegate != nullptr)
    {
        _delegate->addInfo(LevelDataElement::create(infoElement));
    }
}

// @005e54cc
void LevelXMLParser::handleShapesNode(tinyxml2::XMLNode* shapesNode)
{
    int index = 0;
    for (tinyxml2::XMLNode* node = shapesNode->FirstChildElement(); node != nullptr;
         node = node->NextSiblingElement())
    {
        LevelDataElement* element = LevelDataElement::create(node->ToElement());
        _delegate->addShape(element, index);
        index++;
    }
}

// @005e554c
void LevelXMLParser::handleSpecialsNode(tinyxml2::XMLNode* specialsNode)
{
    int index = 0;
    for (tinyxml2::XMLNode* node = specialsNode->FirstChildElement(); node != nullptr;
         node = node->NextSiblingElement())
    {
        LevelDataElement* element = LevelDataElement::create(node->ToElement());
        _delegate->addSpecial(element, index);
        index++;
    }
}

// @005e55cc
void LevelXMLParser::handleGroupsNode(tinyxml2::XMLNode* groupsNode)
{
    int index = 0;
    for (tinyxml2::XMLNode* node = groupsNode->FirstChildElement("g"); node != nullptr;
         node = node->NextSiblingElement("g"))
    {
        LevelDataElement* element = LevelDataElement::create(node->ToElement());
        _delegate->addGroup(element, index);
        index++;
    }
}

// @005e565c
void LevelXMLParser::handleJointsNode(tinyxml2::XMLNode* jointsNode)
{
    int index = 0;
    for (tinyxml2::XMLNode* node = jointsNode->FirstChildElement("j"); node != nullptr;
         node = node->NextSiblingElement("j"))
    {
        LevelDataElement* element = LevelDataElement::create(node->ToElement());
        _delegate->addJoint(element, index);
        index++;
    }
}

// @005e56ec
void LevelXMLParser::handleTriggersNode(tinyxml2::XMLNode* triggersNode)
{
    int index = 0;
    for (tinyxml2::XMLNode* node = triggersNode->FirstChildElement("t"); node != nullptr;
         node = node->NextSiblingElement("t"))
    {
        LevelDataElement* element = LevelDataElement::create(node->ToElement());
        _delegate->addTrigger(element, index);
        index++;
    }
    _delegate->addTriggersComplete();
}
