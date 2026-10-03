#include "SelectBackgroundUIView.h"

#include "ColorInputObject.h"

#include "platform/common/Localization.h"

USING_NS_CC;

namespace {
const float kPickerHeight = 150.0f;  // initWithFrame:CGRectMake(0, 0, scrollW, 150)
const float kPickerRowHeight = 32.0f;
const int kPickerRowTag = 0x5b5;
}  // namespace

SelectBackgroundUIView::SelectBackgroundUIView()
    : _pickerView(nullptr), _colorView(nullptr), _holderView(nullptr), _redSlider(nullptr), _greenSlider(nullptr),
      _blueSlider(nullptr), _rColor(0.0f), _gColor(0.0f), _bColor(0.0f)
{
}

// @ios 1000d1340 (dealloc)
SelectBackgroundUIView::~SelectBackgroundUIView() {}

SelectBackgroundUIView* SelectBackgroundUIView::create(const Rect& frame, int initialBgValue, unsigned int initialColor)
{
    SelectBackgroundUIView* view = new (std::nothrow) SelectBackgroundUIView();
    if (view && view->initWithFrame(frame, initialBgValue, initialColor))
    {
        view->autorelease();
        return view;
    }
    delete view;
    return nullptr;
}

// @ios 1000d0fa8
bool SelectBackgroundUIView::initWithFrame(const Rect& frame, int initialBgValue, unsigned int initialColor)
{
    if (!EditorUIView::initWithFrame(frame)) return false;
    setRColor(static_cast<float>((initialColor >> 16) & 0xff));
    setGColor(static_cast<float>((initialColor >> 8) & 0xff));
    setBColor(static_cast<float>(initialColor & 0xff));

    long row;
    switch (initialBgValue)
    {
    case 0: row = 0; break;
    case 1: row = 1; break;
    case 3: row = 3; break;
    case 4: row = 4; break;
    case -1: row = 5; break;
    case 4001: row = 2; break;
    default: row = -1; break;
    }
    // iOS tests gColor twice (blue is never looked at): "not white" = r != 255 || g != 255.
    const bool notWhite = !(rColor() == 255.0f && gColor() == 255.0f && gColor() == 255.0f);

    _bgs = {Localization::get("NONE"),      Localization::get("GREEN HILLS"), Localization::get("CLOUDS"),
            Localization::get("HONEYCOMB"), Localization::get("BRICKS"),      Localization::get("SOLID COLOR")};
    _menuLabel->setString(uikit::capitalizedString(Localization::get("SELECT BACKGROUND")));

    const float k = uikit::pointsToDesign();
    const Rect pickerFrame(0, 0, _scrollView->frame().size.width, kPickerHeight);
    _pickerView = ui::ListView::create();
    _pickerView->setDirection(ui::ScrollView::Direction::VERTICAL);
    _pickerView->setBounceEnabled(true);
    _pickerView->setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
    _pickerView->setBackGroundColor(Color3B::WHITE);
    _pickerView->setScrollBarEnabled(false);
    _scrollView->addSubview(_pickerView, pickerFrame);  // sizes the list first
    const long components = numberOfComponentsInPickerView(_pickerView);
    for (long component = 0; component < components; ++component)
    {
        const long rows = pickerViewNumberOfRowsInComponent(_pickerView, component);
        for (long r = 0; r < rows; ++r)
        {
            ui::Layout* cell = ui::Layout::create();
            cell->setContentSize(Size(pickerFrame.size.width * k, kPickerRowHeight * k));
            cell->setBackGroundColorType(ui::Layout::BackGroundColorType::SOLID);
            cell->setBackGroundColor(Color3B::WHITE);
            cell->setTag(kPickerRowTag);
            ui::Text* title = uikit::makeLabel(pickerViewTitleForRow(_pickerView, r, component), false, 21.0f, 1);
            title->setContentSize(cell->getContentSize());
            title->setAnchorPoint(Vec2::ZERO);
            title->setPosition(Vec2::ZERO);
            cell->addChild(title);
            cell->setTouchEnabled(true);
            cell->setSwallowTouches(false);
            cell->addClickEventListener([this, r, component](Ref*) {
                selectPickerRow(r);
                pickerViewDidSelectRow(_pickerView, r, component);
            });
            _pickerView->pushBackCustomItem(cell);
        }
    }
    const bool useColor = initialBgValue == 0 && notWhite;
    selectPickerRow(useColor ? 5 : row);
    if (useColor) addColorSelectControls();
    return true;
}

void SelectBackgroundUIView::selectPickerRow(long row)
{
    const Vector<ui::Widget*>& items = _pickerView->getItems();
    for (ssize_t i = 0; i < items.size(); ++i)
    {
        ui::Layout* cell = static_cast<ui::Layout*>(items.at(i));
        cell->setBackGroundColor(i == row ? Color3B(214, 230, 245) : Color3B::WHITE);
    }
    if (row >= 0 && row < items.size()) _pickerView->jumpToItem(row, Vec2(0.5f, 0.5f), Vec2(0.5f, 0.5f));
}

// @ios 1000d13c0
long SelectBackgroundUIView::numberOfComponentsInPickerView(ui::ListView* /*pickerView*/) { return 1; }

// @ios 1000d13c8
long SelectBackgroundUIView::pickerViewNumberOfRowsInComponent(ui::ListView* /*pickerView*/, long /*component*/)
{
    return static_cast<long>(_bgs.size());
}

// @ios 1000d13d8
std::string SelectBackgroundUIView::pickerViewTitleForRow(ui::ListView* /*pickerView*/, long row, long /*component*/)
{
    return _bgs.at(row);
}

// @ios 1000d13ec
void SelectBackgroundUIView::pickerViewDidSelectRow(ui::ListView* /*pickerView*/, long row, long /*component*/)
{
    int bg;
    switch (row)
    {
    case 0:
        if (_delegate) _delegate->editorUIView(this, Value(0), "bg", ValueMapNull);
        setRColor(255.0f);
        setGColor(255.0f);
        setBColor(255.0f);
        removeColorSelectControls();
        return;
    case 1: bg = 1; break;
    case 2: bg = 4001; break;
    case 3: bg = 3; break;
    case 4: bg = 4; break;
    case 5:
        if (_delegate) _delegate->editorUIView(this, Value(0), "bg", ValueMapNull);
        addColorSelectControls();
        return;
    default: return;
    }
    if (_delegate) _delegate->editorUIView(this, Value(bg), "bg", ValueMapNull);
    removeColorSelectControls();
}

// @ios 1000d15c8
void SelectBackgroundUIView::addColorSelectControls()
{
    if (_colorView != nullptr) return;
    const Rect pickerFrame = _scrollView->subviewFrame(_pickerView);
    const Rect scrollFrame = _scrollView->frame();
    _holderView = uikit::View::create(Rect(pickerFrame.origin.x,
                                           static_cast<float>(pickerFrame.origin.y + -30.0 + pickerFrame.size.height),
                                           scrollFrame.size.width, scrollFrame.size.height));
    _holderView->setUserInteractionEnabled(false);  // pass touches to the inputs / scroll view
    _scrollView->addSubview(_holderView);

    _colorView = uikit::View::create(Rect(0, 3, scrollFrame.size.width, 30));
    const float inv255 = 0.003921569f;
    _colorView->setBackgroundColor(uikit::color(static_cast<double>(_rColor * inv255), static_cast<double>(_gColor * inv255),
                                                static_cast<double>(_bColor * inv255), 1.0));
    _colorView->setUserInteractionEnabled(false);
    _holderView->addSubview(_colorView);

    struct Channel
    {
        float y;
        const char* colorLabel;
        const char* property;
        float value;
    };
    const Channel channels[3] = {{36.0f, "r", "rColor", _rColor}, {69.0f, "g", "gColor", _gColor}, {102.0f, "b", "bColor", _bColor}};
    for (const Channel& c : channels)
    {
        const float w = static_cast<float>(_scrollView->frame().size.width + -8.0);
        ColorInputObject* io = ColorInputObject::create(Rect(4, c.y, w, 30), "", c.colorLabel, c.property,
                                                        static_cast<float>(uikit::fcvtzs(c.value)), 0.0f, 255.0f, 0xfe);
        io->setDelegate(this);
        _holderView->addSubview(io);
    }
}

// @ios 1000d1910
void SelectBackgroundUIView::removeColorSelectControls()
{
    if (_holderView == nullptr) return;
    _holderView->removeFromSuperview();
    _holderView = nullptr;
    _colorView = nullptr;
}

// @ios 1000d1950
void SelectBackgroundUIView::closeView(Ref* sender)
{
    const int r = uikit::fcvtzs(_rColor), g = uikit::fcvtzs(_gColor), b = uikit::fcvtzs(_bColor);
    if (delegate()) delegate()->editorUIView(this, Value(g * 0x100 + r * 0x10000 + b), "bgColor", ValueMapNull);
    EditorUIView::closeView(sender);
}

// @ios 1000d1a18
void SelectBackgroundUIView::inputObjectParameter(const std::string& parameter, float value, bool /*updateUndo*/,
                                                  const Value& /*previousValue*/)
{
    setValueForKey(Value(value), parameter);
    if (_colorView == nullptr) return;
    const float inv255 = 0.003921569f;
    _colorView->setBackgroundColor(uikit::color(static_cast<double>(_rColor * inv255), static_cast<double>(_gColor * inv255),
                                                static_cast<double>(_bColor * inv255), 1.0));
}

// @ios 1000d1af0
float SelectBackgroundUIView::rColor() const { return _rColor; }
// @ios 1000d1b00
void SelectBackgroundUIView::setRColor(float rColor) { _rColor = rColor; }
// @ios 1000d1b0c
float SelectBackgroundUIView::gColor() const { return _gColor; }
// @ios 1000d1b1c
void SelectBackgroundUIView::setGColor(float gColor) { _gColor = gColor; }
// @ios 1000d1b28
float SelectBackgroundUIView::bColor() const { return _bColor; }
// @ios 1000d1b38
void SelectBackgroundUIView::setBColor(float bColor) { _bColor = bColor; }

void SelectBackgroundUIView::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "rColor")
        setRColor(value.asFloat());
    else if (key == "gColor")
        setGColor(value.asFloat());
    else if (key == "bColor")
        setBColor(value.asFloat());
    else
        log("SelectBackgroundUIView: setValue:forKey: unknown key %s", key.c_str());
}
