// ONLINE (PC addition): browser specials 0 (Van) and 20 (Bottle) inside groups. Flash VanRef /
// BottleRef are groupable only when non-interactive, so inside a group they are art only: the
// van body with its two tires (VanBodyMC frame 1, VanTireMC at (+-58, 48) behind it) and the
// bottle (BottleMC frame = bottleType). The mobile Van / Bottle ignore groups, so these replace
// them there (FlashSpecialUse::InGroup); outside groups the mobile classes stay.
//
// XML: Van p0 x, p1 y, p2 angle, p3 sleeping, p4 interactive;
//      Bottle p0 x, p1 y, p2 angle, p3 bottleType 1..4, p4 sleeping, p5 interactive.

#include "online/items/FlashSpecials.h"
#include "online/items/MiscItemUtil.h"

#include "LevelDataElement.h"

USING_NS_CC;

namespace online {

namespace {

class GroupArt : public FlashItem
{
public:
    explicit GroupArt(int type) : _type(type) {}
    ~GroupArt() override
    {
        if (_root) _root->release();
    }

    bool init(LevelDataElement* element, b2Body* groupBody, b2Vec2 groupOffset) override
    {
        (void)groupOffset;
        const bool inGroup = groupBody != nullptr || element->stringAttribute("fg") != nullptr;
        _root = Node::create();
        _root->retain();
        _root->setCascadeOpacityEnabled(true);
        if (_type == 0) {
            for (float tx : {-58.0f, 58.0f}) {
                Node* tire = misc::artOr("van_tire", 20.0f, 45.0f, Color4F(0.1f, 0.1f, 0.1f, 1.0f));
                tire->setPosition(misc::localPx(tx, 48.0f));
                _root->addChild(tire, 0);
            }
            _root->addChild(misc::artOr("van_body", 132.0f, 116.0f, Color4F(0.25f, 0.35f, 0.65f, 1.0f)), 1);
        } else {
            const int bottleType = std::max(1, std::min(4, inum(element, "p3", 1)));
            _root->addChild(misc::artOr("bottle_" + std::to_string(bottleType), 10.0f, 29.0f,
                                        Color4F(0.25f, 0.5f, 0.25f, 0.8f)));
        }
        Node* layer = element->stringAttribute("fg") ? flashForegroundLayer() : flashBackgroundLayer();
        if (layer) layer->addChild(_root);
        if (!inGroup) misc::placeFlash(_root, num(element, "p0", 0.0f), num(element, "p1", 0.0f), num(element, "p2", 0.0f));
        return true;
    }

    void paintWithOffsetPoints(Vec2 offset, float rotation) override
    {
        if (!_root) return;
        _root->setPosition(offset);
        _root->setRotation(rotation);
    }

    void setOpacity(float opacity) override
    {
        if (_root) _root->setOpacity((GLubyte)std::lround(std::max(0.0f, std::min(1.0f, opacity)) * 255.0f));
    }

private:
    int _type;
    Node* _root = nullptr;
};

FlashSpecialRegistration s_van(0, [] { return (LevelItem*)new (std::nothrow) GroupArt(0); },
                               FlashSpecialUse::InGroup, /*groupable*/ true);
FlashSpecialRegistration s_bottle(20, [] { return (LevelItem*)new (std::nothrow) GroupArt(20); },
                                  FlashSpecialUse::InGroup, /*groupable*/ true);

}  // namespace

}  // namespace online
