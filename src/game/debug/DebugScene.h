#pragma once

#include "HWWindowDelegate.h"

#include "cocos2d.h"
#include "ui/UISlider.h"

class HWWindow;

// Developer test scene (alert windows, emitter test with "levels/debug/debug.xml", a cocosui slider,
// touch counting). Not reachable from the shipped menus. arm64 sizeof 0x360.
// Bases: Layer at 0, HWWindowDelegate at 0x320 (both public). The HWWindowDelegate overrides get
// primary-vtable slots +0x648 (hwWindowButtonPressed) and +0x650 (hwWindowWasDismissed), in this
// declaration order.
class DebugScene : public cocos2d::Layer, public HWWindowDelegate
{
public:
    // Scene::create() + CREATE_FUNC(DebugScene) inlined + addChild.
    static cocos2d::Scene* createScene();                                      // @005aacc4
    CREATE_FUNC(DebugScene);

    // Grey LayerColor background, scheduleUpdate().
    DebugScene();                                                              // @005aad6c
    void onEnter() override;                                                   // @005aae4c (Layer::onEnter)
    // EventListenerTouchOneByOne (swallowing, fixed priority 100); onTouchBegan counts touches.
    void addTouchInteractivity();                                              // @005aae50
    // HWWindow::createAlertWindow test window with this as delegate.
    void addAlertWindowTest();                                                 // @005aaf1c
    void createEmitterTest();                                                  // @005ab0dc
    void touch(cocos2d::Vec2 location);                                        // @005ab474 (_touchCount += 1)
    // Shows the alert test window when none exists yet.
    void update(float dt) override;                                            // @005ab488
    void createSlider();                                                       // @005ab498
    void sliderEvent(cocos2d::Ref* sender, cocos2d::ui::Slider::EventType type); // @005ab7c0
    // Releases _touchListener.
    ~DebugScene() override;                                                    // @005ab80c (D1), @005ab850 (D0)
    // Returns true without calling Layer::init().
    bool init() override;                                                      // @005ab874

    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override;      // @005ab87c
    void hwWindowWasDismissed(HWWindow* window) override;                      // @005abbd0 (empty)

private:
    // HWWindowDelegate vptr +0x320
    // RE-TODO(@005aad6c): +0x328..+0x350 are unnamed: the constructor zeroes +0x32c..+0x33b only and
    // touch() increments +0x330; nothing else accesses this range (layout placeholders).
    float _unk0x328;                            // +0x328
    float _unk0x32c;                            // +0x32c zeroed
    float _touchCount;                          // +0x330 zeroed, += 1.0f per touch
    float _unk0x334;                            // +0x334 zeroed
    float _unk0x338;                            // +0x338 zeroed
    float _unk0x33c;                            // +0x33c
    void* _unk0x340;                            // +0x340
    void* _unk0x348;                            // +0x348
    cocos2d::EventListenerTouchOneByOne* _touchListener; // +0x350 retained
    HWWindow* _alertWindow;                     // +0x358
};
