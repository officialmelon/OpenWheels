#pragma once

#include "cocos2d.h"

#include <cstdint>
#include <string>
#include <vector>

class PageControlDelegate;

// A horizontal row of page dots (used by LevelSelectMenu for the chapters). The selected dot is
// opaque, the others have opacity 64. Touching / dragging over the row selects a page and reports
// it to the delegate. Member names from the iOS HWPageControl ivars (_dots, _currentPage,
// _hitArea, _delegate).
//
// arm64 sizeof 0x340 (create() allocates 0x340 with new (std::nothrow)).
class PageControl : public cocos2d::Node
{
public:
    // new (std::nothrow) + init + autorelease; init's result is ignored.
    static PageControl* create(std::string spriteFrameName, cocos2d::Vec2 position, float spacing,
                               int numPages, int page);                                 // @005fd524
    // numPages dots centred on position.x, spacing apart; the hit area spans the dots plus half a
    // spacing on each side and +-spacing vertically around position.y. Always returns true.
    bool init(std::string spriteFrameName, cocos2d::Vec2 position, float spacing, int numPages,
              int page);                                                                 // @005fd648

    PageControl();                                     // @005fd9b4  logs "PageControl: constructor"
    ~PageControl() override;                           // @005fda44 (D1), @005fdaf8 (D0); logs "PageControl: destructor"

    void removeListener();                             // @005fdab8
    // Does nothing if page is already selected; does not notify the delegate.
    void setPage(int page);                            // @005fdb1c
    // Non-null: stores it and adds the touch listener. Null: removes the listener.
    void setDelegate(PageControlDelegate* delegate);   // @005fdb9c
    void addListener();                                // @005fdbf8
    void onExit() override;                            // @005fddc8  vptr+0x330

    bool touchBegan(cocos2d::Touch* touch);            // @005fde0c
    void touchMoved(cocos2d::Touch* touch);            // @005fde6c
    void updateIndexWithTouchPosition(cocos2d::Vec2 position);  // @005fdf8c
    void touchEnded(cocos2d::Touch* touch);            // @005fe068
    void touchCancelled(cocos2d::Touch* touch);        // @005fe14c  same as touchEnded

protected:
    // Unsigned as in iOS (`unsigned int _currentPage`); the loops compare it zero-extended.
    unsigned int _currentPage;                         // +0x2f8  -1 (UINT_MAX)
    std::vector<cocos2d::Sprite*> _dots;               // +0x300
    cocos2d::Rect _hitArea;                            // +0x318  world space
    PageControlDelegate* _delegate;                    // +0x328
    cocos2d::EventListenerTouchOneByOne* _touchListener;  // +0x330  retained while registered
    // RE-TODO(@005fd524): 8 bytes at +0x338 are never accessed; kept so arm64 sizeof stays 0x340.
    uint8_t _unk338[8];                                // +0x338
};
