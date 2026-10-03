#pragma once
// iOS LevelXMLParser (TBXML-based) - the editor-facing part. Named EditorLevelXMLParser because
// the game already has the Android LevelXMLParser (tinyxml2, LevelDataElement callbacks), which
// LevelB2D keeps using unchanged. -[EditorLayer addLevelItems] is the iOS client:
//   [[LevelXMLParser alloc] initWithData:levelMO.data delegate:self] parse]
//
// What it does (iOS 1.2.7):
//   parseLevelInfo: <info>: ptm, sw, sh, r (registration: 1 = y measured from the top),
//     cw; then, if the delegate responds, setVersion:background:color:parameters: (v, bg, bgc,
//     [b0, b1, ...]) and addCharacter: with the raw string attributes x, y, c, f, h (x / y
//     replaced by numbers in metres when ptm != 0: x / ptm, (r == 1 ? sh - y : y) / ptm).
//   parseLevelObjects: <shapes>/<sh> -> addShape:objectIndex:, <specials>/<sp> ->
//     addSpecial:objectIndex:, then groups, joints, triggers (EditorLayer implements none of the
//     latter three, so the port does not parse them: same observable behaviour).
//   Dictionaries: "t", "i" (shapes), "p0".."pN" as the raw attribute strings; when ptm != 0 the
//   positional / size parameters are converted to metres (and flipped for r == 1) per type, as
//   shapeDictFromTBXMLElement: / specialDictFromTBXMLElement: do (special types in the iOS bit
//   mask 0x472909765 convert p0/p1; 3 and 4 convert p0..p3; 7 converts p0..p2). Polygon (t 4)
//   and art (t 5) shapes additionally get vertex data; the editor cannot represent them
//   (levelItemRefClass has no ref for 6004 / 6005), so they are reported but skipped by
//   EditorLayer.
//   Editor-written levels have no ptm, so their values pass through as strings.
//
// Values: cocos2d::Value(std::string) for raw attributes, cocos2d::Value(float) for converted
// ones (Special::setProperties reads them with asFloat(), like -floatValue on either type).
// Missing attributes are left out (iOS would throw on setObject:nil forKey:).

#include <string>

#include "base/CCValue.h"

namespace tinyxml2 {
class XMLElement;
class XMLDocument;
}

// iOS informal protocol LevelXMLParserDelegate (the parser checks respondsToSelector:). Only the
// methods EditorLayer implements are declared; defaults do nothing.
class EditorLevelXMLParserDelegate
{
public:
    virtual ~EditorLevelXMLParserDelegate() = default;
    // setVersion:background:color:parameters: (parameters: the b0, b1, ... strings; may be empty).
    virtual void setVersion(float version, unsigned int background, unsigned int color,
                            const cocos2d::ValueVector& parameters) {}
    virtual void addCharacter(const cocos2d::ValueMap& character) {}
    virtual void addShape(const cocos2d::ValueMap& shape, unsigned int objectIndex) {}
    virtual void addSpecial(const cocos2d::ValueMap& special, unsigned int objectIndex) {}
};

class EditorLevelXMLParser
{
public:
    // initWithData:delegate: (the level XML text; the delegate is not retained).
    EditorLevelXMLParser(const std::string& xml, EditorLevelXMLParserDelegate* delegate);  // @ios 100108f10
    ~EditorLevelXMLParser();                                               // @ios 100108fa0

    // Returns false if the XML does not parse (iOS: TBXML yields no root -> nothing happens).
    bool parse();                                                          // @ios 100108ff8
    void parseLevelInfo();                                                 // @ios 10010901c
    void parseLevelObjects();                                              // @ios 100109450
    void addSpecialsWithElement(tinyxml2::XMLElement* specials);           // @ios 100109550
    void addShapesWithElement(tinyxml2::XMLElement* shapes);               // @ios 1001095dc
    cocos2d::ValueMap shapeDictFromTBXMLElement(tinyxml2::XMLElement* shape);     // @ios 100109960
    cocos2d::ValueMap specialDictFromTBXMLElement(tinyxml2::XMLElement* special); // @ios 10010a5f4

    EditorLevelXMLParserDelegate* delegate() { return _delegate; }         // @ios 10010bbc4
    void setDelegate(EditorLevelXMLParserDelegate* delegate) { _delegate = delegate; }  // @ios 10010bbd0

private:
    EditorLevelXMLParserDelegate* _delegate;   // +0x08
    tinyxml2::XMLDocument* _tbxml;             // +0x10  iOS TBXML
    double _stageWidth = 0.0;                  // +0x18  info sw
    double _stageHeight = 0.0;                 // +0x20  info sh
    short _registration = 0;                   // +0x28  info r
    float _ptmRatio = 0.0f;                    // +0x2c  info ptm (0 = values are metres already)
    bool _clockwise = false;                   // +0x30  info cw
    float _version = 0.0f;                     // +0x40  info v
};
