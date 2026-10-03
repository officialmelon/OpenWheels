#pragma once

// Receiver of the level XML elements, called by LevelXMLParser::init in document order.
// Pure interface; implemented by LevelB2D (primary base, vptr at +0x00).
//
// Vtable order (taken from LevelB2D's primary vtable, slots vptr+0x00..+0x30):
// addInfo, addSpecial, addShape, addGroup, addJoint, addTrigger, addTriggersComplete.
// LevelB2D's own destructor slots follow addTriggersComplete, so this interface declares no
// virtual destructor (nothing is ever deleted through it).

class LevelDataElement;

class LevelXMLParserDelegateProtocol
{
public:
    virtual void addInfo(LevelDataElement* info) = 0;                    // +0x00  <info>
    virtual void addSpecial(LevelDataElement* special, int index) = 0;   // +0x08  <specials>/*
    virtual void addShape(LevelDataElement* shape, int index) = 0;       // +0x10  <shapes>/*
    virtual void addGroup(LevelDataElement* group, int index) = 0;       // +0x18  <groups>/g
    virtual void addJoint(LevelDataElement* joint, int index) = 0;       // +0x20  <joints>/j
    virtual void addTrigger(LevelDataElement* trigger, int index) = 0;   // +0x28  <triggers>/t
    virtual void addTriggersComplete() = 0;                              // +0x30  after </triggers>
};
