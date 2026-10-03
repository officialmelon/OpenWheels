#pragma once
// iOS: AlignRefsView : EditorUIView (instanceStart 0x30, instanceSize 0x60).
// "ALIGN OBJECTS" panel, opened by EditorLayer::alignBtnPressed: when 2+ refs are selected
// (frame x = winW/2 - notch, w = winW/2, full height). Four system buttons in the scroll view:
// "up" (22, 0, 44, 44), "down" (88, 0, 44, 44), "left" (22, 44, 44, 44), "right" (88, 44, 44, 44)
// and "OK" at (winW/4, winH - 44, winW/4, 44) (confirmChanges: closes). The button captions are
// hard-coded English in iOS; the port passes them through Localization::get (falls back to the
// literal; "OK" is a real key).
//
// handleAlignBtnPress: computes the union of every ref's refBoundingBox() (cocos coordinates,
// y up) and moves each ref so its box edge touches the union's top / bottom / left / right edge
// (position += edge delta), then sbn->updateSelectionRect(). No undo is registered (iOS).

#include "EditorUIView.h"

class EditorSpriteBatchNode;
class Special;

class AlignRefsView : public EditorUIView
{
public:
    static AlignRefsView* create(const cocos2d::Rect& frame, const cocos2d::Vector<Special*>& specials,
                                 EditorSpriteBatchNode* editorSBN);
    // initWithFrame:specials:editorSBN:
    virtual bool initWithFrame(const cocos2d::Rect& frame, const cocos2d::Vector<Special*>& specials,
                               EditorSpriteBatchNode* editorSBN);                   // @ios 1000dced8

    void handleAlignBtnPress(cocos2d::Ref* sender);                                 // @ios 1000dd204
    void confirmChanges(cocos2d::Ref* sender);                                      // @ios 1000dd698  (removeFromSuperview)

protected:
    AlignRefsView();
    using EditorUIView::initWithFrame;

    EditorSpriteBatchNode* _sbn;           // +0x30  EditorSpriteBatchNode (assign)
    cocos2d::Vector<Special*> _refs;       // +0x38  NSMutableArray
    cocos2d::ui::Button* _upBtn;           // +0x40
    cocos2d::ui::Button* _downBtn;         // +0x48
    cocos2d::ui::Button* _leftBtn;         // +0x50
    cocos2d::ui::Button* _rightBtn;        // +0x58
};
