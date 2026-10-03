#pragma once
// "Quality of Life" options page (PC addition), reached from Options. Built like the original's
// Advanced Options screen (SecondaryMenu + OptionsMenuItem rows), in two columns.

#include "SecondaryMenu.h"

class OptionsMenuItem;

class QoLMenu : public SecondaryMenu
{
public:
    static cocos2d::Scene* createScene();
    CREATE_FUNC(QoLMenu);
    bool init() override;
    void addContent() override;
    void backBtnPressed() override;

private:
    enum Row { RowBlood = 100, RowParticles, RowCamera, RowFps, RowFullscreen, RowUnlockLevels, RowControls };
    std::string labelFor(int row) const;
    void rowPressed(cocos2d::Ref* sender);
    OptionsMenuItem* makeRow(int row);
};
