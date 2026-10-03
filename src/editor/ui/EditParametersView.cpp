#include "EditParametersView.h"

#include <algorithm>
#include <cmath>

#include "EditorUndoManager.h"
#include "Special.h"
#include "platform/common/Localization.h"

USING_NS_CC;

EditParametersView::EditParametersView()
    : _special(nullptr), _keyboardFocusView(nullptr), _keyboardPresent(false), _undoManager(nullptr)
{
}

// @ios 1000c65fc (dealloc)
EditParametersView::~EditParametersView()
{
    uikit::NotificationCenter::removeObserver(this);
    // Port safety: iOS relies on removeFromSuperview having removed the KVO observers.
    removeParameterObservers();
}

EditParametersView* EditParametersView::create(const Rect& frame, const Vector<Special*>& specials)
{
    EditParametersView* view = new (std::nothrow) EditParametersView();
    if (view && view->initWithFrame(frame, specials))
    {
        view->autorelease();
        return view;
    }
    delete view;
    return nullptr;
}

// @ios 1000c6048
bool EditParametersView::initWithFrame(const Rect& frame, const Vector<Special*>& specials)
{
    if (!EditorUIView::initWithFrame(frame)) return false;
    _keyboardFocusView = nullptr;
    _keyboardPresent = false;
    _inputDict.clear();
    _refs = specials;  // [[NSMutableArray alloc] initWithArray:[specials copy]]
    _scrollView->setBackgroundColor(uikit::color(0.800000011920929, 0.800000011920929, 0.800000011920929, 0.25));
    setBackgroundColor(uikit::color(0.7058823704719543, 0.7058823704719543, 0.7058823704719543, 0.5));
    addInputs();
    addParameterObservers();
    using uikit::NotificationCenter;
    NotificationCenter::addObserver(this, uikit::notification::kSelRefChange, nullptr,
                                    [this](void* object, void*) { handleSelectionChange(object); });
    NotificationCenter::addObserver(this, uikit::notification::kRefUIKeysWillChange, nullptr,
                                    [this](void* object, void*) { uiKeysWillChange(object); });
    NotificationCenter::addObserver(this, uikit::notification::kRefUIKeysChanged, nullptr,
                                    [this](void* object, void*) { uiKeysDidChange(object); });
    // UIKeyboardDidShowNotification / UIKeyboardWillHideNotification: no software keyboard on PC.
    NotificationCenter::addObserver(this, uikit::notification::kIOGainsFocus, nullptr,
                                    [this](void* object, void*) { keyboardFocusChange(object); });
    return true;
}

// @ios 1000c6248
void EditParametersView::addInputs()
{
    removeInputObjects();
    if (_refs.empty())
    {
        _menuLabel->setString(uikit::capitalizedString(Localization::get("NOTHING SELECTED")));
        return;
    }
    if (_refs.size() >= 2)
    {
        _menuLabel->setString(uikit::capitalizedString(Localization::get("MULTIPLE OBJECTS")));
        return;
    }
    _special = _refs.at(0);
    const double scrollWidth = _scrollView->frame().size.width;
    _menuLabel->setString(uikit::capitalizedString(_special->name()));
    const std::vector<std::string> keys = _special->propertyKeysForUI();
    float y = 8.0f;
    const float width = static_cast<float>(scrollWidth + -16.0);
    for (const std::string& key : keys)
    {
        InputObject* io = _special->inputObjectForPropertyWithRect(key, Rect(8.0f, y, width, 27.0f));
        if (io == nullptr) continue;
        io->setDelegate(this);
        y = static_cast<float>(static_cast<double>(y) + 8.0 + io->frame().size.height);
        _scrollView->addSubview(io);
        _inputDict[key] = io;
    }
    _scrollView->setScrollContentSize(Size(_scrollView->contentSize().width, y));
}

// @ios 1000c647c
void EditParametersView::removeFromSuperview()
{
    uikit::NotificationCenter::removeObserver(this);
    removeParameterObservers();
    _refs.clear();
    EditorUIView::removeFromSuperview();
}

// @ios 1000c64e8
void EditParametersView::removeInputObjects()
{
    for (auto& entry : _inputDict) entry.second->removeFromSuperview();
    _inputDict.clear();
}

// @ios 1000c6670
void EditParametersView::uiKeysWillChange(void* /*notificationObject*/) { removeParameterObservers(); }

// @ios 1000c6674
void EditParametersView::uiKeysDidChange(void* /*notificationObject*/)
{
    addInputs();
    addParameterObservers();
}

// @ios 1000c6698
void EditParametersView::handleSelectionChange(void* notificationObject)
{
    log("refs %p", notificationObject);
    removeParameterObservers();
    _refs.clear();
    if (notificationObject != nullptr) _refs = *static_cast<Vector<Special*>*>(notificationObject);
    addInputs();
    addParameterObservers();
}

// @ios 1000c6724
void EditParametersView::addParameterObservers()
{
    for (Special* ref : _refs)
    {
        for (const std::string& key : ref->propertyKeysForUI())
        {
            ref->addObserver(this, key, [this](const std::string& keyPath, Special* object, const Value& newValue) {
                observeValueForKeyPath(keyPath, object, newValue);
            });
        }
    }
}

// @ios 1000c68c8
void EditParametersView::removeParameterObservers()
{
    for (Special* ref : _refs)
    {
        for (const std::string& key : ref->propertyKeysForUI()) ref->removeObserver(this, key);
    }
}

// @ios 1000c6a64
void EditParametersView::observeValueForKeyPath(const std::string& keyPath, Special* /*object*/, const Value& newValue)
{
    auto it = _inputDict.find(keyPath);
    InputObject* io = it != _inputDict.end() ? it->second : nullptr;
    const float value = newValue.asFloat();  // [[change objectForKey:@"new"] floatValue]
    if (io != nullptr && io->propertyValue() != value) io->setPropertyValue(value, false, Value::Null);
}

// @ios 1000c6ae8
void EditParametersView::keyboardDidShow(float keyboardHeight)
{
    // RE-TODO(@1000c6ae8): the iOS inset is computed from the keyboard frame converted against
    // this view and the scroll view frames; PC has no software keyboard, so nothing calls this.
    _keyboardPresent = true;
    const float inset = keyboardHeight;
    _scrollView->setContentInset(0, 0, inset, 0);
    _scrollView->setScrollIndicatorInsets(0, 0, inset, 0);
    scrollToKeyboardFocus();
}

// @ios 1000c6bc4
void EditParametersView::scrollToKeyboardFocus()
{
    if (_keyboardFocusView == nullptr) return;
    // Scroll up (never down) so the focused row's bottom + 18 pt is visible above the inset.
    const Rect focus = _keyboardFocusView->frame();
    const float visible = _scrollView->frame().size.height - _scrollView->contentInset().size.width;  // bottom inset
    float target = focus.getMaxY() + 18.0f - visible;
    if (target <= 0.0f) target = 0.0f;
    if (static_cast<float>(uikit::fcvtzs(target)) < _scrollView->contentOffset().y)
        _scrollView->setContentOffset(Vec2(0.0f, static_cast<float>(uikit::fcvtzs(target))), false);
}

// @ios 1000c6c98
void EditParametersView::keyboardWillHide()
{
    _keyboardFocusView = nullptr;
    _keyboardPresent = false;
    _scrollView->setContentInset(0, 0, 0, 0);
    _scrollView->setScrollIndicatorInsets(0, 0, 0, 0);
    _scrollView->setContentOffset(Vec2::ZERO, false);
}

// @ios 1000c6d18
void EditParametersView::keyboardFocusChange(void* notificationObject)
{
    _keyboardFocusView = static_cast<InputObject*>(notificationObject);
    if (_keyboardPresent) scrollToKeyboardFocus();
}

// @ios 1000c6d68
void EditParametersView::textViewDidEndEditing(ui::EditBox* /*textView*/) {}

// @ios 1000c6d70
bool EditParametersView::textFieldShouldReturn(ui::EditBox* /*textField*/) { return false; }

// @ios 1000c6d8c
void EditParametersView::textViewDidChange(ui::EditBox* /*textView*/) {}

// @ios 1000c6d90
void EditParametersView::inputObjectParameter(const std::string& parameter, float value, bool updateUndo,
                                              const Value& previousValue)
{
    if (!updateUndo)
    {
        for (Special* ref : _refs)
        {
            if (ref->valueForKey(parameter).asFloat() != value) ref->setValueForKey(Value(value), parameter);
        }
    }
    else
    {
        if (_undoManager != nullptr) _undoManager->beginUndoGrouping();
        for (Special* ref : _refs)
        {
            // [[undoManager prepareWithInvocationTarget:ref] setValue:old forKey:parameter]
            const Value old = previousValue.isNull() ? ref->valueForKey(parameter) : previousValue;
            if (_undoManager != nullptr)
            {
                const std::string key = parameter;
                _undoManager->prepareWithInvocationTarget(ref, [old, key](Special* s) { s->setValueForKey(old, key); });
            }
            ref->setValueForKey(Value(value), parameter);
        }
        if (_undoManager != nullptr) _undoManager->endUndoGrouping();
        uikit::NotificationCenter::postNotification(uikit::notification::kUndoStackUpdated, _undoManager);
    }
    uikit::NotificationCenter::postNotification(uikit::notification::kSelRectChanged, nullptr);
}

// @ios 1000c70a8
void EditParametersView::inputObjectParameterLabelTapped(const std::string& parameter)
{
    if (parameter == "angle" && !_refs.empty())
    {
        Special* ref = _refs.front();
        if (_undoManager != nullptr)
        {
            _undoManager->beginUndoGrouping();
            const Value old(ref->getRotation());
            const std::string key = parameter;
            _undoManager->prepareWithInvocationTarget(ref, [old, key](Special* s) { s->setValueForKey(old, key); });
            _undoManager->endUndoGrouping();
        }
        uikit::NotificationCenter::postNotification(uikit::notification::kUndoStackUpdated, _undoManager);
        const float rotation = ref->getRotation();
        float angle;
        if (std::fmod(rotation, 45.0f) == 0.0f)
            angle = std::fmod(ref->getRotation() + 45.0f, 360.0f);
        else
            angle = std::ceil(rotation * 0.0222222228f) * 45.0f;  // ceilf(rotation / 45) * 45
        ref->setAngle(Value(angle));
    }
    uikit::NotificationCenter::postNotification(uikit::notification::kSelRectChanged, nullptr);
}

// @ios 1000c7214
void EditParametersView::confirmChanges(Ref* /*sender*/) { removeFromSuperview(); }

// @ios 1000c7218
void EditParametersView::cancelChanges(Ref* /*sender*/) { removeFromSuperview(); }

// @ios 1000c721c
const Vector<InputObject*>& EditParametersView::inputObjects() const { return _inputObjects; }

// @ios 1000c722c
EditorUndoManager* EditParametersView::undoManager() const { return _undoManager; }

// @ios 1000c723c
void EditParametersView::setUndoManager(EditorUndoManager* undoManager) { _undoManager = undoManager; }
