#pragma once

#include <string>

// Analytics front end (no base class, no RTTI, arm64 sizeof 3; owned by Settings).
//
// In Android 1.1.3 submitAction() already sends nothing: it only assembles a std::map of
// {"category", "label", "number"} and destroys it (the analytics SDK call is gone), so the PC build
// keeps exactly that. "send_feedback_disabled" (UserDefault, bool) is the user's opt-out.
//
// The iOS ancestor is HWTracker (_sendFeedback, _internetReachable, ...).
class Tracker
{
public:
    // _sendFeedback = !UserDefault "send_feedback_disabled", _internetReachable = true.
    Tracker();                                                                 // @00649428
    ~Tracker();                                                                // @00649464

    void setInternetReachable(bool internetReachable);                         // @00649468
    bool getInternetReachable();                                               // @00649470
    void setSendFeedback(bool sendFeedback);                                   // @00649478
    bool getSendFeedback();                                                    // @00649490
    bool showStartupMessage();                                                 // @00649498 (true)
    bool promptRating(bool force);                                             // @006494a0 (false)
    // Callers pass value = -1 when there is no number. action is not used.
    void submitAction(std::string category, std::string action, std::string label,
                      int value);                                              // @006494a8

private:
    bool _sendFeedback;                         // +0x0
    bool _internetReachable;                    // +0x1
    // RE-TODO(@00649428): zeroed by the constructor, never accessed.
    bool _unk0x2;                               // +0x2
};
