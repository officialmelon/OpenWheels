#pragma once

// LevelXMLParser: parses the level XML text (tinyxml2) and feeds every element, in document
// order, to a LevelXMLParserDelegateProtocol (LevelB2D). Plain class, no vtable, sizeof 8.
// LevelB2D::init creates it with `new LevelXMLParser()` (value-initialised), calls init and
// deletes it again.

#include <string>

namespace tinyxml2 {
class XMLElement;
class XMLNode;
}

class LevelXMLParserDelegateProtocol;

class LevelXMLParser
{
public:
    // Returns false when xml is empty or does not parse / has no root element.
    bool init(std::string xml, LevelXMLParserDelegateProtocol* delegate);

    void handleInfoNode(tinyxml2::XMLElement* infoElement);
    void handleShapesNode(tinyxml2::XMLNode* shapesNode);      // every child element
    void handleSpecialsNode(tinyxml2::XMLNode* specialsNode);  // every child element
    void handleGroupsNode(tinyxml2::XMLNode* groupsNode);      // children "g"
    void handleJointsNode(tinyxml2::XMLNode* jointsNode);      // children "j"
    void handleTriggersNode(tinyxml2::XMLNode* triggersNode);  // children "t", then addTriggersComplete

private:
    LevelXMLParserDelegateProtocol* _delegate;  // +0x00
};
