#include "ContactListener.h"

#include "LevelItem.h"

// Dispatch rule shared by the four Box2D callbacks: listeners are visited in registration order
// (index loop re-reading size() each iteration); each element is copied (including its unused
// std::function) and the registered fixture is passed first, the other fixture of the contact
// second. A listener matching neither fixture is skipped.

// @005a590c
ContactListener::ContactListener()
{
}

// @005a593c (D1), @005a5b3c (D0)
ContactListener::~ContactListener()
{
    // members destroyed in reverse order: end, begin, preSolve, postSolve listeners
}

// @005a5b60
void ContactListener::BeginContact(b2Contact* contact)
{
    b2Fixture* fixtureA = contact->GetFixtureA();
    b2Fixture* fixtureB = contact->GetFixtureB();
    for (int i = 0; i < _beginContactListeners.size(); i++)
    {
        Listener listener = _beginContactListeners[i];
        if (listener.fixture == fixtureA)
        {
            listener.levelItem->beginContact(fixtureA, fixtureB, contact);
        }
        else if (listener.fixture == fixtureB)
        {
            listener.levelItem->beginContact(fixtureB, fixtureA, contact);
        }
    }
}

// @005a5d04
void ContactListener::EndContact(b2Contact* contact)
{
    b2Fixture* fixtureA = contact->GetFixtureA();
    b2Fixture* fixtureB = contact->GetFixtureB();
    for (int i = 0; i < _endContactListeners.size(); i++)
    {
        Listener listener = _endContactListeners[i];
        if (listener.fixture == fixtureA)
        {
            listener.levelItem->endContact(fixtureA, fixtureB, contact);
        }
        else if (listener.fixture == fixtureB)
        {
            listener.levelItem->endContact(fixtureB, fixtureA, contact);
        }
    }
}

// @005a5ea8
void ContactListener::PreSolve(b2Contact* contact, const b2Manifold* oldManifold)
{
    b2Fixture* fixtureA = contact->GetFixtureA();
    b2Fixture* fixtureB = contact->GetFixtureB();
    for (int i = 0; i < _preSolveListeners.size(); i++)
    {
        Listener listener = _preSolveListeners[i];
        if (listener.fixture == fixtureA)
        {
            listener.levelItem->preSolve(fixtureA, fixtureB, contact, oldManifold);
        }
        else if (listener.fixture == fixtureB)
        {
            listener.levelItem->preSolve(fixtureB, fixtureA, contact, oldManifold);
        }
    }
}

// @005a6054
void ContactListener::PostSolve(b2Contact* contact, const b2ContactImpulse* impulse)
{
    b2Fixture* fixtureA = contact->GetFixtureA();
    b2Fixture* fixtureB = contact->GetFixtureB();
    for (int i = 0; i < _postSolveListeners.size(); i++)
    {
        Listener listener = _postSolveListeners[i];
        if (listener.fixture == fixtureA)
        {
            listener.levelItem->postSolve(fixtureA, fixtureB, contact, impulse);
        }
        else if (listener.fixture == fixtureB)
        {
            listener.levelItem->postSolve(fixtureB, fixtureA, contact, impulse);
        }
    }
}

// @005a6200
void ContactListener::addBeginContactListener(b2Fixture* fixture, LevelItem* levelItem)
{
    addToVector(fixture, levelItem, _beginContactListeners);
}

// @005a6208
void ContactListener::addToVector(b2Fixture* fixture, LevelItem* levelItem,
                                  std::vector<Listener>& listeners)
{
    // One Listener is reused for the duplicate scan (copy-assigned from every element) and then
    // pushed, so the pushed callback is a copy of the last element's (always empty in practice).
    Listener listener;
    for (int i = 0; i < listeners.size(); i++)
    {
        listener = listeners[i];
        if (listener.fixture == fixture && listener.levelItem == levelItem)
        {
            return;
        }
    }
    listener.fixture = fixture;
    listener.levelItem = levelItem;
    listeners.push_back(listener);
}

// @005a6458
void ContactListener::addEndContactListener(b2Fixture* fixture, LevelItem* levelItem)
{
    addToVector(fixture, levelItem, _endContactListeners);
}

// @005a6460
void ContactListener::addPreSolveListener(b2Fixture* fixture, LevelItem* levelItem)
{
    addToVector(fixture, levelItem, _preSolveListeners);
}

// @005a6468
void ContactListener::addPostSolveListener(b2Fixture* fixture, LevelItem* levelItem)
{
    addToVector(fixture, levelItem, _postSolveListeners);
}

// @005a6470
void ContactListener::removeBeginContactListener(b2Fixture* fixture, LevelItem* levelItem)
{
    removeFromVector(fixture, levelItem, _beginContactListeners);
}

// @005a6478
void ContactListener::removeFromVector(b2Fixture* fixture, LevelItem* levelItem,
                                       std::vector<Listener>& listeners)
{
    // Removes every matching entry (the iterator stays in place after an erase).
    for (std::vector<Listener>::iterator it = listeners.begin(); it != listeners.end();)
    {
        Listener listener = *it;
        if (listener.fixture == fixture && listener.levelItem == levelItem)
        {
            it = listeners.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// @005a66d0
void ContactListener::removeEndContactListener(b2Fixture* fixture, LevelItem* levelItem)
{
    removeFromVector(fixture, levelItem, _endContactListeners);
}

// @005a66d8
void ContactListener::removePreSolveListener(b2Fixture* fixture, LevelItem* levelItem)
{
    removeFromVector(fixture, levelItem, _preSolveListeners);
}

// @005a66e0
void ContactListener::removePostSolveListener(b2Fixture* fixture, LevelItem* levelItem)
{
    removeFromVector(fixture, levelItem, _postSolveListeners);
}
