// LevelTextView -- UITextView stand-in for the E5 screens (see LevelTextView.h).
#include "LevelTextView.h"

#include <algorithm>

USING_NS_CC;

namespace {

const float kInsetPoints = 6.0f;   // UITextView textContainerInset (~8) / lineFragmentPadding

// The editable field: a TextFieldTTF whose keyboard input is routed through LevelTextView so
// every edit can be vetoed first. The caret always sits at the end (arrow/home/end keys are
// ignored), so edits are appends and backspaces exactly as LevelTextView reports them.
class LevelTextFieldTTF : public TextFieldTTF
{
public:
    static LevelTextFieldTTF* create(LevelTextView* owner, const Size& dimensions, float fontSize)
    {
        LevelTextFieldTTF* field = new (std::nothrow) LevelTextFieldTTF();
        if (field && field->initWithPlaceHolder("", dimensions, TextHAlignment::LEFT, "Arial", fontSize))
        {
            field->_owner = owner;
            field->autorelease();
            return field;
        }
        delete field;
        return nullptr;
    }

    void insertText(const char* text, size_t len) override
    {
        if (_owner)
        {
            _owner->handleInsertText(std::string(text, len));
        }
    }

    void deleteBackward() override
    {
        if (_owner)
        {
            _owner->handleDeleteBackward();
        }
    }

    void controlKey(EventKeyboard::KeyCode keyCode) override
    {
    }

    bool attached() const
    {
        return _isAttachWithIME;
    }

    LevelTextView* _owner = nullptr;
};

}  // namespace

LevelTextView* LevelTextView::create(const Rect& frame, float pointSize, bool editable)
{
    LevelTextView* view = new (std::nothrow) LevelTextView();
    if (view && view->initWithFrame(frame, pointSize, editable))
    {
        view->autorelease();
        return view;
    }
    delete view;
    return nullptr;
}

LevelTextView::LevelTextView()
    : _editable(false),
      _pointSize(14.0f),
      _textColor(Color4B::BLACK),
      _delegate(nullptr),
      _field(nullptr),
      _scroller(nullptr),
      _label(nullptr),
      _touchListener(nullptr)
{
}

LevelTextView::~LevelTextView()
{
    if (_field)
    {
        static_cast<LevelTextFieldTTF*>(_field)->_owner = nullptr;
        _field->setDelegate(nullptr);
        _field->detachWithIME();
    }
}

bool LevelTextView::initWithFrame(const Rect& frame, float pointSize, bool editable)
{
    if (!uikit::View::initWithFrame(frame))
    {
        return false;
    }
    _editable = editable;
    _pointSize = pointSize;
    setClippingEnabled(true);

    float scale = uikit::pointsToDesign();
    Size size(frame.size.width * scale, frame.size.height * scale);
    float inset = kInsetPoints * scale;
    if (_editable)
    {
        _field = LevelTextFieldTTF::create(this, Size(size.width - 2.0f * inset, 0.0f), _pointSize * scale);
        _field->setDelegate(this);
        _field->setCursorEnabled(true);
        _field->setTextColor(_textColor);
        _field->setAnchorPoint(Vec2(0.0f, 1.0f));
        _field->setPosition(Vec2(inset, size.height - inset));
        addChild(_field);
        // Tap = becomeFirstResponder (UITextView gains focus on touch).
        addTouchEventListener([this](Ref*, ui::Widget::TouchEventType type) {
            if (type == ui::Widget::TouchEventType::ENDED)
            {
                becomeFirstResponder();
            }
        });
    }
    else
    {
        _scroller = ui::ScrollView::create();
        _scroller->setDirection(ui::ScrollView::Direction::VERTICAL);
        _scroller->setContentSize(size);
        _scroller->setBounceEnabled(true);
        _scroller->setScrollBarEnabled(true);
        addChild(_scroller);
        _label = Label::createWithSystemFont("", "Arial", _pointSize * scale,
                                             Size(size.width - 2.0f * inset, 0.0f), TextHAlignment::LEFT);
        _label->setTextColor(_textColor);
        _label->setAnchorPoint(Vec2(0.0f, 1.0f));
        _scroller->addChild(_label);
    }
    layoutText();
    return true;
}

const std::string& LevelTextView::text() const
{
    return _text;
}

void LevelTextView::setText(const std::string& text)
{
    _text = text;
    layoutText();
}

size_t LevelTextView::length() const
{
    return lengthUTF16(_text);
}

size_t LevelTextView::lengthUTF16(const std::string& utf8)
{
    // UTF-16 code units: one per code point, two for code points >= 0x10000 (4-byte UTF-8).
    size_t units = 0;
    for (size_t i = 0; i < utf8.size(); i++)
    {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if ((c & 0xC0) == 0x80)
        {
            continue;  // continuation byte
        }
        units += (c >= 0xF0) ? 2 : 1;
    }
    return units;
}

void LevelTextView::setDelegate(LevelTextViewDelegate* delegate)
{
    _delegate = delegate;
}

void LevelTextView::setTextColor(const Color4B& color)
{
    _textColor = color;
    if (_field)
    {
        _field->setTextColor(color);
    }
    if (_label)
    {
        _label->setTextColor(color);
    }
}

bool LevelTextView::isEditable() const
{
    return _editable;
}

bool LevelTextView::becomeFirstResponder()
{
    return _field && _field->attachWithIME();
}

bool LevelTextView::resignFirstResponder()
{
    return _field && _field->detachWithIME();
}

bool LevelTextView::isFirstResponder() const
{
    return _field && static_cast<LevelTextFieldTTF*>(_field)->attached();
}

void LevelTextView::handleInsertText(const std::string& text)
{
    // One edit per line piece and per "\n" (a Return key press arrives alone as "\n").
    size_t start = 0;
    while (start <= text.size())
    {
        size_t newline = text.find('\n', start);
        std::string piece = text.substr(start, newline == std::string::npos ? std::string::npos : newline - start);
        if (!piece.empty())
        {
            if (!_delegate || _delegate->textViewShouldChangeTextInRange(this, length(), 0, piece))
            {
                _text += piece;
                layoutText();
                if (_delegate)
                {
                    _delegate->textViewDidChange(this);
                }
            }
        }
        if (newline == std::string::npos)
        {
            break;
        }
        if (!_delegate || _delegate->textViewShouldChangeTextInRange(this, length(), 0, "\n"))
        {
            _text += "\n";
            layoutText();
            if (_delegate)
            {
                _delegate->textViewDidChange(this);
            }
        }
        start = newline + 1;
    }
}

void LevelTextView::handleDeleteBackward()
{
    if (_text.empty())
    {
        return;
    }
    size_t charStart = _text.size() - 1;
    while (charStart > 0 && (static_cast<unsigned char>(_text[charStart]) & 0xC0) == 0x80)
    {
        charStart--;
    }
    size_t deletedUnits = lengthUTF16(_text.substr(charStart));
    size_t total = length();
    if (_delegate && !_delegate->textViewShouldChangeTextInRange(this, total - deletedUnits, deletedUnits, ""))
    {
        return;
    }
    _text.erase(charStart);
    layoutText();
    if (_delegate)
    {
        _delegate->textViewDidChange(this);
    }
}

bool LevelTextView::onTextFieldAttachWithIME(TextFieldTTF* sender)
{
    if (_delegate)
    {
        _delegate->textViewDidBeginEditing(this);
    }
    return false;  // allow
}

bool LevelTextView::onTextFieldDetachWithIME(TextFieldTTF* sender)
{
    if (_delegate)
    {
        _delegate->textViewDidEndEditing(this);
    }
    return false;  // allow
}

bool LevelTextView::onTextFieldInsertText(TextFieldTTF* sender, const char* text, size_t nLen)
{
    return false;  // not used: LevelTextFieldTTF routes input through handleInsertText
}

bool LevelTextView::onTextFieldDeleteBackward(TextFieldTTF* sender, const char* delText, size_t nLen)
{
    return false;  // not used: see handleDeleteBackward
}

void LevelTextView::layoutText()
{
    if (_field)
    {
        _field->setString(_text);
    }
    if (_label && _scroller)
    {
        float scale = uikit::pointsToDesign();
        float inset = kInsetPoints * scale;
        _label->setString(_text);
        Size viewSize = _scroller->getContentSize();
        float innerHeight = std::max(viewSize.height, _label->getContentSize().height + 2.0f * inset);
        _scroller->setInnerContainerSize(Size(viewSize.width, innerHeight));
        _label->setPosition(Vec2(inset, innerHeight - inset));
        _scroller->jumpToTop();
    }
}
