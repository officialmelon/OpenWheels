#pragma once
// EDITOR (browser features, PC addition): a browser special the iOS editor has no ref for
// (table, NPC, text box, chair, TV, boombox, toilet, trash can, food, glass, meteor, buildings,
// rail, chain, token, cannon, paddle, sign). Its attributes come from FlashCatalog (the Flash
// editor's ref classes); values are kept in Flash units (px, degrees, 0/1) under the Flash
// attribute names and written back unchanged as <sp t="type" p0.. pN> (the text box caption as a
// <p7> child, as the Flash editor does).
//
// KVC keys: every catalog attribute name, plus "flashX" / "flashY" (position in px) and "angle".
// The art is the Flash editor's own (generated/flash/ed_*.png, tools/assets/flash_items/
// editor.txt); without it a labelled box of the item's footprint is drawn.

#include <map>
#include <string>

#include "Special.h"
#include "FlashCatalog.h"

namespace tinyxml2 {
class XMLElement;
}

class FlashSpecialRef : public Special
{
public:
    static FlashSpecialRef* create(int type);
    bool initWithType(int type);

    const flashed::SpecialInfo* info() const { return _info; }
    int type() const { return _info->type; }
    float param(const std::string& key) const;
    const std::string& caption() const { return _caption; }
    bool interactiveItem() const;   // "interactive" attribute (true when the item has none)

    std::string displayName() const { return _info->name; }

    // Browser XML (p0 / p1 are px positions; groups pass their local offset through `origin`).
    void readFlash(const tinyxml2::XMLElement* e);
    // Appends ` p0="" ...` attributes to `attrs`; the caption (if any) goes to `childXml`.
    void writeFlash(std::string& attrs, std::string& childXml, const cocos2d::Vec2& positionPx, float angle);

    // Special
    std::vector<std::string> propertyKeysForUI() override;
    InputObject* inputObjectForPropertyWithRect(const std::string& property, const cocos2d::Rect& rect) override;
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;
    cocos2d::ValueMap properties() override;
    void setProperties(const cocos2d::ValueMap& properties) override;
    void setRotation(float rotation) override;
    void updateOverlayWithNode(cocos2d::DrawNode* node) override;

protected:
    FlashSpecialRef() = default;
    void setParam(const std::string& key, float value);
    float clampParam(const std::string& key, float value) const;
    void rebuildArt();
    void updateCounts();

    const flashed::SpecialInfo* _info = nullptr;
    std::map<std::string, float> _params;
    std::string _caption = "HERE'S SOME TEXT";
    cocos2d::Node* _art = nullptr;
    cocos2d::Size _footprint;     // stage units, centred on the position (text: from top-left)
    bool _drawnBox = false;       // no art: the overlay draws a box
};
