#pragma once
// iOS: SelectBackgroundUIView : EditorUIView (instanceStart 0x30, instanceSize 0x80).
// "SELECT BACKGROUND" panel: a one-column picker of backgrounds plus, for "SOLID COLOR",
// three ColorInputObjects (r, g, b) and a colour swatch. Opened by
// EditorLayer::selectBgBtnPressed: with initialBgValue = EditorLayer _bgIndex and
// initialColor = _bgColor (0xRRGGBB); delegate = the EditorLayer.
//
// Picker rows (localization keys) -> "bg" value sent through
// delegate->editorUIView(this, Value(int), "bg", nullptr):
//   0 NONE -> 0 (r, g, b reset to 255, colour controls removed)
//   1 GREEN HILLS -> 1      2 CLOUDS -> 4001 (0xfa1)      3 HONEYCOMB -> 3      4 BRICKS -> 4
//   5 SOLID COLOR -> 0 (colour controls added)
// Initial row: bg 0 with a non-white colour -> 5 (+ colour controls); otherwise 0 -> 0, 1 -> 1,
// 4001 -> 2, 3 -> 3, 4 -> 4, -1 -> 5, anything else -> -1 (no selection).
// Colour controls (holder view under the picker; picker frame (0, 0, scrollW, 150)):
//   swatch view at y 3 filled with (r, g, b) / 255; ColorInputObjects at x 4, y 36 / 69 / 102,
//   height 30, 0..255, 254 segments, colorLabel "r"/"g"/"b", property "rColor"/"gColor"/"bColor",
//   delegate = this. Exact widths: Phase I (from the assembly).
// Closing sends "bgColor" = (r << 16) + (g << 8) + b (ints) to the delegate, then posts
// "editor_view_closed" (EditorUIView::closeView).
// Localization keys: "SELECT BACKGROUND", "NONE", "GREEN HILLS", "CLOUDS", "HONEYCOMB",
// "BRICKS", "SOLID COLOR", "CLOSE".

#include <string>
#include <vector>

#include "EditorUIView.h"
#include "InputObject.h"

class SelectBackgroundUIView : public EditorUIView, public InputObjectDelegate
{
public:
    static SelectBackgroundUIView* create(const cocos2d::Rect& frame, int initialBgValue,
                                          unsigned int initialColor);
    // initWithFrame:initialBgValue:initialColor:
    virtual bool initWithFrame(const cocos2d::Rect& frame, int initialBgValue,
                               unsigned int initialColor);                          // @ios 1000d0fa8

    // UIPickerViewDataSource / UIPickerViewDelegate (the ListView is filled from these).
    long numberOfComponentsInPickerView(cocos2d::ui::ListView* pickerView);                          // @ios 1000d13c0  (1)
    long pickerViewNumberOfRowsInComponent(cocos2d::ui::ListView* pickerView, long component);       // @ios 1000d13c8
    std::string pickerViewTitleForRow(cocos2d::ui::ListView* pickerView, long row, long component);  // @ios 1000d13d8
    void pickerViewDidSelectRow(cocos2d::ui::ListView* pickerView, long row, long component);        // @ios 1000d13ec

    void addColorSelectControls();                                                  // @ios 1000d15c8
    void removeColorSelectControls();                                               // @ios 1000d1910
    void closeView(cocos2d::Ref* sender) override;                                  // @ios 1000d1950

    // InputObjectDelegate: [self setValue:@(value) forKey:parameter], then recolour the swatch.
    void inputObjectParameter(const std::string& parameter, float value, bool updateUndo,
                              const cocos2d::Value& previousValue) override;        // @ios 1000d1a18

    // NSNumber properties (stored as float; read back with intValue = truncation).
    float rColor() const;                                                           // @ios 1000d1af0
    void setRColor(float rColor);                                                   // @ios 1000d1b00
    float gColor() const;                                                           // @ios 1000d1b0c
    void setGColor(float gColor);                                                   // @ios 1000d1b1c
    float bColor() const;                                                           // @ios 1000d1b28
    void setBColor(float bColor);                                                   // @ios 1000d1b38
    // KVC used by inputObjectParameter ("rColor" / "gColor" / "bColor").
    void setValueForKey(const cocos2d::Value& value, const std::string& key);

protected:
    SelectBackgroundUIView();
    ~SelectBackgroundUIView() override;                                             // @ios 1000d1340  dealloc
    using EditorUIView::initWithFrame;

    // UIPickerView selectRow:inComponent:animated: (row highlight; -1 = none).
    void selectPickerRow(long row);

    std::vector<std::string> _bgs;         // +0x30  NSArray of localized row titles
    cocos2d::ui::ListView* _pickerView;    // +0x38  UIPickerView
    uikit::View* _colorView;               // +0x40  UIView (swatch)
    uikit::View* _holderView;              // +0x48  UIView
    uikit::Slider* _redSlider;       // +0x50  UISlider (declared, unused in 1.2.7)
    uikit::Slider* _greenSlider;     // +0x58  UISlider (declared, unused in 1.2.7)
    uikit::Slider* _blueSlider;      // +0x60  UISlider (declared, unused in 1.2.7)
    float _rColor;                         // +0x68  NSNumber
    float _gColor;                         // +0x70  NSNumber
    float _bColor;                         // +0x78  NSNumber
};
