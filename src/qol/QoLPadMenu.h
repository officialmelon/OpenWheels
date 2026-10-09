#pragma once
// PAD (PC addition): QoL "controller" page. Every controller action with its three input slots
// (src/qol/PadBindings.h): select a slot, then press the controller input to bind to it (select
// the waiting slot again to clear it; nothing pressed for a few seconds cancels). Below: rumble
// strength, tilt steering (phones / handhelds), phone vibration (Android) and "reset to
// defaults"; above: the connected controllers. Pushed from the QoL page; the back button pops it.

#include <vector>

#include "SecondaryMenu.h"
#include "base/CCEventKeyboard.h"

class QoLRowItem;
class OptionsMenuItem;

class QoLPadMenu : public SecondaryMenu
{
public:
    static cocos2d::Scene* createScene();
    CREATE_FUNC(QoLPadMenu);
    bool init() override;
    void addContent() override;
    void backBtnPressed() override;
    void onExit() override;
    void update(float dt) override;

private:
    void slotPressed(cocos2d::Ref* sender);
    void optionPressed(cocos2d::Ref* sender);
    void keyPressed(cocos2d::EventKeyboard::KeyCode key, cocos2d::Event* event);
    void startCapture(int index);
    void stopCapture();
    void refresh();
    std::string optionLabel(int tag) const;

    std::vector<QoLRowItem*> _slots;   // action * kPadSlots + slot
    std::vector<OptionsMenuItem*> _options;
    cocos2d::Label* _status = nullptr;
    int _capturing = -1;               // index into _slots, -1 when not waiting for an input
    float _captureTime = 0.0f;
    std::string _statusText;
    cocos2d::EventListenerKeyboard* _keys = nullptr;
};
