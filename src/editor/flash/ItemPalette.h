#pragma once
// EDITOR (browser features, PC addition): the "add items" panel in the style of the online level
// browser: every item the editor can place (the iOS catalog plus the browser game's items, logic
// and drawing tools, EditorSettings::sectionedLevelItemIDs), as a scrolling grid of tiles under
// section headings. Tapping a tile picks it (EditorLayer::handleMenuTouch places it).

#include <functional>

#include "cocos2d.h"

namespace flashed {

class ItemPalette : public cocos2d::Node
{
public:
    static ItemPalette* create(const cocos2d::Size& size);
    std::function<void(int levelItemID)> onPick;
    std::function<void()> onClose;

protected:
    bool init(const cocos2d::Size& size);
    void build();
    void layout();
    cocos2d::Node* makeTile(int itemID, const cocos2d::Size& size);

    cocos2d::Node* _content = nullptr;
    cocos2d::Rect _viewport;
    float _scroll = 0.0f;
    float _contentHeight = 0.0f;
    bool _dragged = false;
    cocos2d::Vec2 _down;
    struct Tile
    {
        cocos2d::Node* node;
        int id;
    };
    std::vector<Tile> _tiles;
    struct Item
    {
        cocos2d::Node* node;
        float x;       // from the viewport's left
        float top;     // from the content's top
        bool heading;
    };
    std::vector<Item> _items;   // tiles and headings
};

}  // namespace flashed
