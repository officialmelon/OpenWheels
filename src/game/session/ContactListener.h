#pragma once

// ContactListener: the session's b2ContactListener. LevelItems register (fixture, item) pairs
// per callback kind; Box2D callbacks are forwarded to LevelItem::beginContact/endContact/
// preSolve/postSolve with the registered fixture first. sizeof 0x68 (arm64).

#include <functional>
#include <vector>

#include "Box2D/Box2D.h"

class LevelItem;

// Element of the ContactListener vectors (sizeof 0x40). The std::function is copied around
// but never set nor called in 1.1.3.
struct Listener
{
    b2Fixture* fixture;              // +0x00
    LevelItem* levelItem;            // +0x08
    std::function<void()> callback;  // +0x10 RE-TODO: signature unknown (never instantiated)
};

class ContactListener : public b2ContactListener
{
public:
    ContactListener();
    virtual ~ContactListener();

    virtual void BeginContact(b2Contact* contact) override;
    virtual void EndContact(b2Contact* contact) override;
    virtual void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) override;
    virtual void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) override;

    void addBeginContactListener(b2Fixture* fixture, LevelItem* levelItem);
    void addToVector(b2Fixture* fixture, LevelItem* levelItem, std::vector<Listener>& listeners);
    void addEndContactListener(b2Fixture* fixture, LevelItem* levelItem);
    void addPreSolveListener(b2Fixture* fixture, LevelItem* levelItem);
    void addPostSolveListener(b2Fixture* fixture, LevelItem* levelItem);
    void removeBeginContactListener(b2Fixture* fixture, LevelItem* levelItem);
    void removeFromVector(b2Fixture* fixture, LevelItem* levelItem,
                          std::vector<Listener>& listeners);
    void removeEndContactListener(b2Fixture* fixture, LevelItem* levelItem);
    void removePreSolveListener(b2Fixture* fixture, LevelItem* levelItem);
    void removePostSolveListener(b2Fixture* fixture, LevelItem* levelItem);

protected:
    std::vector<Listener> _postSolveListeners;     // +0x08
    std::vector<Listener> _preSolveListeners;      // +0x20
    std::vector<Listener> _beginContactListeners;  // +0x38
    std::vector<Listener> _endContactListeners;    // +0x50
};
