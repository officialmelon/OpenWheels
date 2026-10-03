#pragma once
// LevelTextView: the UITextViews of LoadLevelViewController (read-only, scrolling description) and
// SaveLevelViewController (editable name / description). Port class, no iOS counterpart; a
// uikit::View so it takes iOS point frames like the other editor views.
//
// Why not ui::EditBox (E4's UITextField mapping): SaveLevelViewController's limits live in
// textView:shouldChangeTextInRange:replacementText:, which needs a veto before each edit and the
// Return key as "\n". Editable views therefore use a cocos2d::TextFieldTTF subclass (keyboard
// input through the GLView IME bridge) that hands every insertion / backspace to
// handleInsertText / handleDeleteBackward, mapped to UITextViewDelegate:
//   shouldChangeTextInRange   before each insertion: range (length, 0) -- the caret stays at the
//                             end (arrow keys ignored), input always appends; before each backspace: range
//                             (length - 1, 1) with replacement ""; false drops the edit.
//                             Return arrives as replacement "\n".
//   textViewDidChange         after each accepted edit. Not sent for setText() (UIKit does not
//                             either; the iOS code calls textViewDidChange: itself).
//   didBegin/didEndEditing    IME attach / detach.
// Lengths are NSString lengths: UTF-16 code units.

#include "UIKitCompat.h"

#include <string>

class LevelTextView;

class LevelTextViewDelegate
{
public:
    virtual ~LevelTextViewDelegate() {}
    virtual void textViewDidBeginEditing(LevelTextView* textView) {}
    virtual void textViewDidChange(LevelTextView* textView) {}
    virtual void textViewDidEndEditing(LevelTextView* textView) {}
    virtual bool textViewShouldChangeTextInRange(LevelTextView* textView, size_t location,
                                                 size_t length, const std::string& replacement)
    {
        return true;
    }
};

class LevelTextView : public uikit::View, public cocos2d::TextFieldDelegate
{
public:
    // frame in points; pointSize = UIFont size (14 in both nibs).
    static LevelTextView* create(const cocos2d::Rect& frame, float pointSize, bool editable);
    bool initWithFrame(const cocos2d::Rect& frame, float pointSize, bool editable);
    using uikit::View::initWithFrame;

    const std::string& text() const;
    void setText(const std::string& text);
    size_t length() const;                                   // UTF-16 units
    static size_t lengthUTF16(const std::string& utf8);

    void setDelegate(LevelTextViewDelegate* delegate);
    void setTextColor(const cocos2d::Color4B& color);
    bool isEditable() const;

    // UIResponder
    bool becomeFirstResponder();
    bool resignFirstResponder();
    bool isFirstResponder() const;

    // Keyboard input of the editable field (called by its TextFieldTTF): one UITextView edit
    // per call, vetoed by textViewShouldChangeTextInRange.
    void handleInsertText(const std::string& text);
    void handleDeleteBackward();

    // cocos2d::TextFieldDelegate (attach/detach = begin/end editing; insert/delete go through
    // handleInsertText/handleDeleteBackward instead)
    bool onTextFieldAttachWithIME(cocos2d::TextFieldTTF* sender) override;
    bool onTextFieldDetachWithIME(cocos2d::TextFieldTTF* sender) override;
    bool onTextFieldInsertText(cocos2d::TextFieldTTF* sender, const char* text, size_t nLen) override;
    bool onTextFieldDeleteBackward(cocos2d::TextFieldTTF* sender, const char* delText, size_t nLen) override;

protected:
    LevelTextView();
    ~LevelTextView() override;
    void layoutText();

    std::string _text;
    bool _editable;
    float _pointSize;
    cocos2d::Color4B _textColor;
    LevelTextViewDelegate* _delegate;                   // weak
    cocos2d::TextFieldTTF* _field;                      // editable
    cocos2d::ui::ScrollView* _scroller;                 // read-only
    cocos2d::Label* _label;                             // read-only
    cocos2d::EventListenerTouchOneByOne* _touchListener;  // tap inside = becomeFirstResponder
};
