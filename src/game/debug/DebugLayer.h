#pragma once

#include "cocos2d.h"
#include "extensions/GUI/CCScrollView/CCTableView.h"

#include <string>
#include <vector>

// Parameter type of Gameplay::handleDebugLayerAction (@005bb028, an empty function). No enumerator
// is visible anywhere in the binary.
// RE-TODO(@005bb028): enumerators unknown (DebugLayer acts on its menu itself, see tableCellTouched).
enum DebugLayerAction
{
};

// In-game QA overlay: a TableView listing debug commands (complete levels, level victory, timestep,
// joint breaks, debug draw, drag, stats, gravity, impulse strength, ...). Created by
// Gameplay::debugBtnPressed through the inlined CREATE_FUNC (new (std::nothrow), Layer::init(),
// autorelease) and added at z 11. arm64 sizeof 0x360.
//
// Bases: Layer at 0, TableViewDataSource at 0x320, TableViewDelegate at 0x328 (all public).
// The overrides of the secondary bases get primary-vtable slots in this declaration order:
// tableCellTouched (+0x648), tableCellAtIndex (+0x650), tableCellSizeForIndex (+0x658),
// numberOfCellsInTableView (+0x660).
class DebugLayer : public cocos2d::Layer,
                   public cocos2d::extension::TableViewDataSource,
                   public cocos2d::extension::TableViewDelegate
{
public:
    CREATE_FUNC(DebugLayer);

    // Fills _labels, sets the table geometry, then addMenu().
    DebugLayer();                                                              // @005a879c
    // TableView (dataSource/delegate = this) + translucent black LayerColor behind it.
    void addMenu();                                                            // @005a97e0
    ~DebugLayer() override;                                                    // @005a9a00 (D1), @005a9aa4 (D0)

    // Runs command cell->getIdx() against the current Session / LevelB2D / characters and updates
    // the cell label (tag 911) for toggles.
    void tableCellTouched(cocos2d::extension::TableView* table,
                          cocos2d::extension::TableViewCell* cell) override;   // @005a9b18
    // TableViewCell with an Arial 60 Label (tag 911) centred in the cell.
    cocos2d::extension::TableViewCell* tableCellAtIndex(cocos2d::extension::TableView* table,
                                                        ssize_t idx) override; // @005aa80c
    std::string getLabel(ssize_t idx);                                         // @005aaa94
    cocos2d::Size tableCellSizeForIndex(cocos2d::extension::TableView* table,
                                        ssize_t idx) override;                 // @005aaab0
    ssize_t numberOfCellsInTableView(cocos2d::extension::TableView* table) override; // @005aaad0

private:
    // TableViewDataSource vptr +0x320, TableViewDelegate vptr +0x328
    std::vector<std::string> _labels;           // +0x330 command names, one per cell
    // RE-TODO(@005a879c): set to 1.0f by the constructor, never read. (The iOS DebugLayer has
    // impulse_, alignSpriteIndex, charIndex and anchorIncrementAmount ivars; no clear match.)
    float _unk0x348;                            // +0x348
    // RE-TODO: 4 bytes never initialised or accessed (layout placeholder).
    int _unk0x34c;                              // +0x34c
    // Strength passed to CharacterB2D::debug*Break(): -1.0f "WEAK" (initial) / 1.0f "STRONG".
    float _impulse;                             // +0x350
    float _cellWidth;                           // +0x354 800
    float _tableHeight;                         // +0x358 visibleSize.height * 0.7
    float _cellHeight;                          // +0x35c 80
};
