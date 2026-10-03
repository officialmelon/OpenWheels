#include "Tracker.h"

#include "cocos2d.h"

USING_NS_CC;

// PC platform policy: analytics are dropped. The Android 1.1.3 submitAction() already sent nothing
// (it assembled a {"category", "label", "number"} map and destroyed it); here the body is empty.
// The opt-out flag and the reachability flag keep their original behaviour because the options menus
// read and write them.

// @00649428
Tracker::Tracker()
{
    _internetReachable = true;
    _unk0x2 = false;
    _sendFeedback = !UserDefault::getInstance()->getBoolForKey("send_feedback_disabled");
}

// @00649464
Tracker::~Tracker()
{
}

// @00649468
void Tracker::setInternetReachable(bool internetReachable)
{
    _internetReachable = internetReachable;
}

// @00649470
bool Tracker::getInternetReachable()
{
    return _internetReachable;
}

// @00649478
void Tracker::setSendFeedback(bool sendFeedback)
{
    if (_sendFeedback != sendFeedback)
    {
        _sendFeedback = sendFeedback;
    }
}

// @00649490
bool Tracker::getSendFeedback()
{
    return _sendFeedback;
}

// @00649498
bool Tracker::showStartupMessage()
{
    return true;
}

// @006494a0
bool Tracker::promptRating(bool force)
{
    return false;
}

// @006494a8
void Tracker::submitAction(std::string category, std::string action, std::string label, int value)
{
    // PC: analytics dropped (see top of file).
}
