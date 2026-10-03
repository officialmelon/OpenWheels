#pragma once

class PageControl;

// Listener interface for PageControl (the row of page dots under the level select).
//
// arm64: a bare vptr (sizeof 8), no virtual destructor. The only implementer is LevelSelectMenu
// (subobject at +0x300); its PageControlDelegate secondary vtable has exactly one slot:
//   [0] pageControl(PageControl*, int)
// The class has typeinfo but no vtable or out-of-line body of its own in the binary (nothing ever
// needed them), so pure vs. empty default body is not observable. An empty inline body is used,
// matching the other delegate interfaces of this codebase (HWWindowDelegate, AdControllerDelegate).
class PageControlDelegate
{
public:
    // Called by PageControl when a touch moved the selection to another dot; page is the new index.
    virtual void pageControl(PageControl* control, int page) {}
};
