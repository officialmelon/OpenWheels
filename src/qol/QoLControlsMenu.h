#pragma once
// QoL "controls" page (PC addition): every keyboard action with its two key slots
// (src/qol/KeyBindings.h). Click a slot, then press a key to bind it (Esc cancels, Backspace /
// Delete clears the slot); "reset to defaults" restores the original keys. Pushed from the QoL
// page; the back button pops it.

#include <vector>

#include "SecondaryMenu.h"
#include "base/CCEventKeyboard.h"

class QoLRowItem;

class QoLControlsMenu : public SecondaryMenu
{
public:
    static cocos2d::Scene* createScene();
    CREATE_FUNC(QoLControlsMenu);
    bool init() override;
    void addContent() override;
    void backBtnPressed() override;
    void onExit() override;

private:
    void slotPressed(cocos2d::Ref* sender);
    void resetPressed(cocos2d::Ref* sender);
    void keyPressed(cocos2d::EventKeyboard::KeyCode key, cocos2d::Event* event);
    void refresh();

    std::vector<QoLRowItem*> _slots;  // action * kKeySlots + slot
    int _capturing = -1;              // index into _slots, -1 when not waiting for a key
    cocos2d::EventListenerKeyboard* _keys = nullptr;
};
