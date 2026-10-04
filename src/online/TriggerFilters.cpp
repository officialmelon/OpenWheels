// ONLINE (PC addition): see TriggerFilters.h.
#include "online/TriggerFilters.h"

#include <vector>

#include "DestructionListener.h"
#include "Session.h"
#include "Settings.h"

namespace online {

void filterToFixed(b2Filter* f, float v, bool groupStyle)
{
    if (v > 1.84f)
    {
        if (f->categoryBits == 8)
        {
            f->categoryBits = 24;
            if (f->maskBits == 8)
            {
                f->maskBits = 56;
            }
            else if (f->groupIndex == 0)
            {
                f->groupIndex = -10;
            }
        }
        else if (f->categoryBits == 16)
        {
            f->maskBits = 48;
            f->groupIndex = 0;
        }
        else if (f->categoryBits == 32)
        {
            f->categoryBits = 48;
        }
    }
    else if (!groupStyle && f->maskBits == 0xffff && f->groupIndex == 0)
    {
        f->groupIndex = -10;
        f->categoryBits = 24;
    }
}

void filterToNonFixed(b2Filter* f, float v, bool groupStyle)
{
    if (v > 1.84f)
    {
        if (f->categoryBits == 24)
        {
            f->categoryBits = 8;
            if (f->maskBits == 56)
            {
                f->maskBits = 8;
            }
            else if (f->groupIndex == -10)
            {
                f->groupIndex = 0;
            }
        }
        else if (f->categoryBits == 16)
        {
            f->maskBits = 16;
            f->groupIndex = -322;
        }
        else if (f->categoryBits == 48)
        {
            f->categoryBits = 32;
        }
    }
    else if (!groupStyle && f->groupIndex == -10 && f->maskBits == 0xffff)
    {
        f->groupIndex = 0;
        f->categoryBits = 8;
    }
}

void filterForCollision(b2Filter* f, bool* sensor, int c, bool fixed, float v)
{
    switch (c)
    {
        case 2:  // only fixed / level
            f->categoryBits = 8;
            f->maskBits = 8;
            f->groupIndex = 0;
            if (v > 1.84f && fixed)
            {
                f->categoryBits = 24;
                f->maskBits = 56;
            }
            break;
        case 3:  // nothing
            if (v < 1.82f)
            {
                f->categoryBits = 1;
                f->maskBits = 1;
                f->groupIndex = -10;
            }
            else
            {
                f->categoryBits = 0;
                f->maskBits = 0;
                f->groupIndex = 0;
            }
            break;
        case 4:  // everything but other shapes of this kind
            f->categoryBits = 8;
            f->maskBits = 0xffff;
            f->groupIndex = -321;
            if (v > 1.84f && fixed)
            {
                f->categoryBits = 24;
            }
            break;
        case 5:
            f->categoryBits = 16;
            f->maskBits = 16;
            f->groupIndex = -322;
            if (v > 1.84f && fixed)
            {
                f->maskBits = 48;
                f->groupIndex = 0;
            }
            break;
        case 6:
            f->categoryBits = 16;
            f->maskBits = 16;
            f->groupIndex = 0;
            if (v > 1.84f)
            {
                f->categoryBits = fixed ? 48 : 32;
                f->maskBits = 48;
            }
            break;
        case 7:  // only characters
            f->categoryBits = 15;
            f->maskBits = 3840;
            f->groupIndex = 0;
            break;
        default:  // everything (groupIndex untouched unless fixed, as in Flash)
            f->categoryBits = 8;
            f->maskBits = 0xffff;
            if (fixed)
            {
                f->categoryBits = 24;
                f->groupIndex = -10;
            }
            break;
    }
    *sensor = c == 3 && v < 1.82f;
}

void destroyJointsOf(b2Body* body, const std::function<bool(b2Joint*)>& filter)
{
    std::vector<b2Joint*> joints;
    for (b2JointEdge* edge = body->GetJointList(); edge != nullptr; edge = edge->next)
    {
        if (!filter || filter(edge->joint))
        {
            joints.push_back(edge->joint);
        }
    }
    Session* session = Settings::getInstance()->getCurrentSession();
    b2World* world = body->GetWorld();
    for (b2Joint* joint : joints)
    {
        if (session != nullptr && session->getDestructionListener() != nullptr)
        {
            session->getDestructionListener()->SayGoodbye(joint);
        }
        world->DestroyJoint(joint);
    }
}

}  // namespace online
