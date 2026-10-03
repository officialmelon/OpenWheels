#pragma once

#include "cocos2d.h"
#include "AdControllerDelegate.h"
#include "HWWindowDelegate.h"

#include <cstdint>

class HWWindow;

// Face shown by the mascot. Stored in UserDefault "mascot_state". The values come from the
// binary; the enumerator names are chosen from the sprite frames.
enum MascotState
{
    MascotStateDefault = 0,    // "mascot_default.png"; the right arm waves
    MascotStateAnguished = 1,  // "mascot_anguish.png" while a grunt sound plays (after a poke)
    MascotStateDead = 2,       // "mascot_dead.png" after _hitsToKill pokes
};

// Hat drawn on the mascot. The values come from the binary; names from the sprite frames.
enum MascotFlair
{
    MascotFlairMortarboard = 0,  // "mascot_mortorboard.png" at (3,22) (no caller in this build)
    MascotFlairTinyHat = 1,      // "mascot_tinyhat.png" at (0,15): necromancer level 3
};

// The little mascot inside HWWindow alerts (HWWindow::init creates it when asked for one). Poking
// it makes it grunt and flail; after enough pokes it dies, and reviving it was offered through a
// rewarded video ad (the window offering it is never created in this build).
//
// arm64 sizeof 0x3b0 (HWWindow's inlined create() allocates 0x3b0). HWWindowDelegate subobject at
// +0x2f8, AdControllerDelegate subobject at +0x300.
class Mascot : public cocos2d::Node, public HWWindowDelegate, public AdControllerDelegate
{
public:
    // Inlined into HWWindow::init in the binary: new (std::nothrow), virtual init(),
    // autorelease / virtual delete on failure.
    CREATE_FUNC(Mascot);

    Mascot();                                          // @005ea924
    ~Mascot() override;                                // @005ea9bc (D1), @005eaa64 (D0)

    void removeListeners();                            // @005eaa24
    bool init() override;                              // @005eaa88  vptr+0x4f8
    void addListeners();                               // @005eaea8
    void setMascotState(MascotState state);            // @005eafd0
    void addFlair(MascotFlair flair);                  // @005eb5cc
    // Hit test against the body sprite's texture rect.
    bool touchBegan(cocos2d::Touch* touch);            // @005eb740
    void touchEnded(cocos2d::Touch* touch);            // @005eb7b4
    void update(float dt) override;                    // @005eb89c  vptr+0x3d8
    // Runs 1s after rewardedVideoDidUpdateStatus(): revives the mascot or shows the failure alert.
    void rewardedVideoComplete();                      // @005ebb54
    void dismissAdRelatedWindows();                    // @005ec3b8

    // Overrides of secondary-base virtuals. Declaration order = order of their new primary-vtable
    // slots (vptr+0x528, +0x530, +0x538).
    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;    // @005ec3fc (thunk @005ec65c)
    void hwWindowWasDismissed(HWWindow* window) override;                    // @005ec664 (thunk @005ec668), empty
    void rewardedVideoDidUpdateStatus(RewardedVideoStatus status) override;  // @005ec66c (thunk @005ec7a4)

protected:
    int _numGrunts;                                    // +0x308  16 ("mascot/grunt1".."mascot/grunt16")
    int _gruntAudioId;                                 // +0x30c  -1
    int _lastGruntIndex;                               // +0x310  -1
    bool _gruntDurationKnown;                          // +0x314  false
    int _hitCount;                                     // +0x318  0
    int _hitsToKill;                                   // +0x31c  4
    // RE-TODO(@005ea924): bool zeroed by the constructor, never used.
    bool _unk320;                                      // +0x320
    MascotState _mascotState;                          // +0x324  MascotStateDefault
    // RE-TODO(@005ea924): pair initialised to (0, 100.0f) by the constructor, never used.
    float _unk328;                                     // +0x328
    float _unk32c;                                     // +0x32c
    float _defaultGruntDuration;                       // +0x330  2.0; used while AudioEngine reports -1
    float _gruntDuration;                              // +0x334  not initialised by the constructor
    float _gruntTimeRemaining;                         // +0x338  not initialised by the constructor
    cocos2d::Sprite* _body;                            // +0x340  "mascot_body.png"
    cocos2d::Sprite* _face;                            // +0x348  default / anguish / dead
    cocos2d::Sprite* _armLeft;                         // +0x350  "mascot_arm_left.png"
    cocos2d::Sprite* _armRight;                        // +0x358  "mascot_arm_right.png"
    cocos2d::Sprite* _legLeft;                         // +0x360  "mascot_leg_left.png"
    cocos2d::Sprite* _legRight;                        // +0x368  "mascot_leg_right.png"
    cocos2d::Sprite* _flair;                           // +0x370
    float _wavePhase;                                  // +0x378  radians, +0.05 per frame
    cocos2d::EventListenerTouchOneByOne* _touchListener;  // +0x380  retained while registered
    // Window offering to revive the mascot. Only compared / cleared in this build, never created.
    HWWindow* _reviveWindow;                           // +0x388
    HWWindow* _loadingAdWindow;                        // +0x390  "Loading ad..."
    HWWindow* _adFailedWindow;                         // +0x398  "Failure" alert
    RewardedVideoStatus _rewardedVideoStatus;          // +0x3a0  not initialised by the constructor
    // RE-TODO(@005ea924): 12 bytes at +0x3a4 are never accessed; kept so arm64 sizeof stays 0x3b0.
    uint8_t _unk3a4[12];                               // +0x3a4
};
