#pragma once

// DestructionListener: the session's b2DestructionListener. LevelItems register the joints/
// fixtures/bodies they hold; SayGoodbye forwards to LevelItem::jointWillBeDestroyed /
// fixtureWillBeDestroyed and drops the entry. sizeof 0x50 (arm64).

#include <utility>
#include <vector>

#include "Box2D/Box2D.h"

class LevelItem;

class DestructionListener : public b2DestructionListener
{
public:
    DestructionListener();
    virtual ~DestructionListener();

    void addFixtureListener(b2Fixture* fixture, LevelItem* levelItem);  // ignores duplicates
    void addJointListener(b2Joint* joint, LevelItem* levelItem);
    void addBodyListener(b2Body* body, LevelItem* levelItem);
    void removeFixtureListener(LevelItem* levelItem, b2Fixture* fixture);
    void removeJointListener(LevelItem* levelItem, b2Joint* joint);
    void removeBodyListener(LevelItem* levelItem, b2Body* body);
    void removeListeners(LevelItem* levelItem);  // joints and fixtures only (sic)

    virtual void SayGoodbye(b2Joint* joint) override;
    virtual void SayGoodbye(b2Fixture* fixture) override;
    void SayGoodbye(b2Body* body);  // not a Box2D callback: never called by b2World

protected:
    std::vector<std::pair<b2Joint*, LevelItem*>> _jointListeners;      // +0x08
    std::vector<std::pair<b2Fixture*, LevelItem*>> _fixtureListeners;  // +0x20
    std::vector<std::pair<b2Body*, LevelItem*>> _bodyListeners;        // +0x38
};
