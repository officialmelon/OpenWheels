#pragma once
// EDITOR (browser features, PC addition): the property inspector of the editor, replacing the
// iOS EditParametersView panel with the look of the online level browser's detail panel. It
// shows the selected item's attributes (every ref: the iOS editor's inputs, mapped to modern
// controls; browser items / triggers / joints / groups: their Flash attributes), the trigger
// actions of a target (Flash's "trigger N action M" rows), a trigger's targets with their action
// lists, and the item's functions (group, ungroup, vehicle, link targets...).
//
// Edits go through the ref's KVC exactly like EditParametersView (undo grouping, KVO refresh,
// "sel_rect_changed"), so undo / redo work the same.

#include <functional>
#include <string>
#include <vector>

#include "cocos2d.h"
#include "FlashCatalog.h"

class Special;

namespace flashed {

struct RowSpec
{
    enum Kind { Field, Slider, Toggle, Choice, Sound, Color, Text, Header, Button, Info };
    Kind kind = Field;
    std::string key;
    std::string label;
    std::string help;
    float min = 0.0f;
    float max = 0.0f;
    int segments = 0;
    bool limits = false;
    bool angle = false;
    bool decimals = true;
    bool allowNone = false;                    // colour: "none" (-1)
    std::vector<std::string> choices;
    std::vector<float> choiceValues;           // value of each choice
    std::string buttonText;
    std::string buttonColor = "blue";
    std::string secondButtonText;              // optional small button on the row (remove, unlink)
    std::string secondKey;
};

class Inspector : public cocos2d::Node
{
public:
    // size in design units; placed by the caller.
    static Inspector* create(const cocos2d::Size& size);
    void setSelection(const cocos2d::Vector<Special*>& refs);
    std::function<void()> onClose;
    // Functions ("fn:group", "fn:ungroup", "fn:vehicle", "fn:unvehicle", "link:<uid>", ...).
    std::function<void(const std::string&)> onFunction;
    void rebuild();
    void refreshValues();

    // The row a key shows as (false: no row).
    static bool specFor(Special* ref, const std::string& key, RowSpec* spec);
    // The keys shown for a ref, in order.
    static std::vector<std::string> keysFor(Special* ref);

protected:
    bool init(const cocos2d::Size& size);
    void onEnter() override;
    void onExit() override;
    void clearRows();
    void addRow(const RowSpec& spec);
    void layoutRows();
    void scrollBy(float dy);
    void setValue(const std::string& key, const cocos2d::Value& value, bool undo, const cocos2d::Value& previous);
    bool inViewport(const cocos2d::Vec2& world) const;

    struct Row
    {
        RowSpec spec;
        cocos2d::Node* node = nullptr;
        float height = 0.0f;
        std::function<void()> refresh;
    };
    std::vector<Row> _rows;
    cocos2d::Vector<Special*> _selection;
    Special* _ref = nullptr;            // the single edited ref (nullptr: none / several)
    cocos2d::Sprite* _panel = nullptr;
    cocos2d::Label* _title = nullptr;
    cocos2d::Label* _subtitle = nullptr;
    cocos2d::ClippingRectangleNode* _clip = nullptr;
    cocos2d::Node* _content = nullptr;
    cocos2d::Rect _viewport;            // in this node's space
    float _scroll = 0.0f;
    float _contentHeight = 0.0f;
    bool _dragging = false;
    cocos2d::Vec2 _dragStart;
};

}  // namespace flashed
