#include "DestructionListener.h"

#include "LevelItem.h"

// All loops use an unsigned index re-checked against size() after each step; an erase keeps the
// index (the next element slides into place).

// @005abd34
DestructionListener::DestructionListener()
{
}

// @005abd60 (D1), @005abdd8 (D0)
DestructionListener::~DestructionListener()
{
    // Only the joint and fixture lists are cleared explicitly (the body list is just freed).
    _jointListeners.clear();
    _fixtureListeners.clear();
}

// @005abdfc
void DestructionListener::addFixtureListener(b2Fixture* fixture, LevelItem* levelItem)
{
    // Unlike addJointListener/addBodyListener, an identical pair is not added twice.
    for (int i = 0; i < _fixtureListeners.size(); i++)
    {
        if (_fixtureListeners[i].first == fixture && _fixtureListeners[i].second == levelItem)
        {
            return;
        }
    }
    _fixtureListeners.push_back(std::make_pair(fixture, levelItem));
}

// @005abf3c
void DestructionListener::addJointListener(b2Joint* joint, LevelItem* levelItem)
{
    _jointListeners.push_back(std::make_pair(joint, levelItem));
}

// @005ac054
void DestructionListener::addBodyListener(b2Body* body, LevelItem* levelItem)
{
    _bodyListeners.push_back(std::make_pair(body, levelItem));
}

// @005ac16c
void DestructionListener::removeFixtureListener(LevelItem* levelItem, b2Fixture* fixture)
{
    for (unsigned int i = 0; i < _fixtureListeners.size();)
    {
        if (_fixtureListeners[i].second == levelItem && _fixtureListeners[i].first == fixture)
        {
            _fixtureListeners.erase(_fixtureListeners.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

// @005ac1fc
void DestructionListener::removeJointListener(LevelItem* levelItem, b2Joint* joint)
{
    for (unsigned int i = 0; i < _jointListeners.size();)
    {
        if (_jointListeners[i].second == levelItem && _jointListeners[i].first == joint)
        {
            _jointListeners.erase(_jointListeners.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

// @005ac28c
void DestructionListener::removeBodyListener(LevelItem* levelItem, b2Body* body)
{
    for (unsigned int i = 0; i < _bodyListeners.size();)
    {
        if (_bodyListeners[i].second == levelItem && _bodyListeners[i].first == body)
        {
            _bodyListeners.erase(_bodyListeners.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

// @005ac31c
void DestructionListener::removeListeners(LevelItem* levelItem)
{
    // Joints, then fixtures. Body listeners of the item are left in place (sic).
    for (unsigned int i = 0; i < _jointListeners.size();)
    {
        if (_jointListeners[i].second == levelItem)
        {
            _jointListeners.erase(_jointListeners.begin() + i);
        }
        else
        {
            i++;
        }
    }
    for (unsigned int i = 0; i < _fixtureListeners.size();)
    {
        if (_fixtureListeners[i].second == levelItem)
        {
            _fixtureListeners.erase(_fixtureListeners.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

// @005ac3f4
void DestructionListener::SayGoodbye(b2Joint* joint)
{
    for (unsigned int i = 0; i < _jointListeners.size();)
    {
        if (_jointListeners[i].first == joint)
        {
            _jointListeners[i].second->jointWillBeDestroyed(joint);
            _jointListeners.erase(_jointListeners.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

// @005ac498
void DestructionListener::SayGoodbye(b2Fixture* fixture)
{
    for (unsigned int i = 0; i < _fixtureListeners.size();)
    {
        if (_fixtureListeners[i].first == fixture)
        {
            _fixtureListeners[i].second->fixtureWillBeDestroyed(fixture);
            _fixtureListeners.erase(_fixtureListeners.begin() + i);
        }
        else
        {
            i++;
        }
    }
}

// @005ac53c
void DestructionListener::SayGoodbye(b2Body* body)
{
    for (unsigned int i = 0; i < _bodyListeners.size();)
    {
        if (_bodyListeners[i].first == body)
        {
            _bodyListeners[i].second->bodyWillBeDestroyed(body);
            _bodyListeners.erase(_bodyListeners.begin() + i);
        }
        else
        {
            i++;
        }
    }
}
