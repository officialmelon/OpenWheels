#pragma once
// ACCESSIBILITY (PC addition): the "accessibility" page, reached from Quality of Life. A colour
// filter (colour-blindness corrections, high contrast), the on-screen controls (show / hide and how
// solid they are), no camera shake, and haptics (controller rumble and the phone's vibration).
// Pushed from the QoL page; the back button pops it.

#include "SecondaryMenu.h"

class OptionsMenuItem;

class QoLAccessibilityMenu : public SecondaryMenu
{
public:
    static cocos2d::Scene* createScene();
    CREATE_FUNC(QoLAccessibilityMenu);
    bool init() override;
    void addContent() override;
    void backBtnPressed() override;

private:
    enum Row
    {
        RowColorFilter = 300,
        RowTouchControls,
        RowReduceShake,
        RowHideTimer,
        RowToughness,
        RowRumble,
        RowPhoneVibration,
    };
    std::string labelFor(int row) const;
    void rowPressed(cocos2d::Ref* sender);
    OptionsMenuItem* makeRow(int row);
};
