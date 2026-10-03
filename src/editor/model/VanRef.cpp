#include "VanRef.h"

#include <cmath>

USING_NS_CC;

// @ios 1000ec584
bool VanRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    setCanDragModify(false);
    setLevelItemID(0);

    // iOS reads Session.ptmRatio for every use.
    const float ptm = sessionPtmRatio();
    const float halfOffset = ptm * 0.648f;

    // port: every child is art shown at its iOS point size (editorArtScale).
    const float art = editorArtScale();
    Sprite* body = Sprite::createWithSpriteFrameName("e_van.png");
    body->setScale(art);
    body->setPosition(Vec2(-(ptm * 0.648f), 0.0f));
    addChild(body);
    body = Sprite::createWithSpriteFrameName("e_van.png");
    body->setScaleY(art);
    body->setScaleX(-1.0f * art);
    body->setPosition(Vec2(halfOffset, 0.0f));
    addChild(body);

    Sprite* wheel = Sprite::createWithSpriteFrameName("e_van_wheel.png");
    wheel->setScale(art);
    wheel->setPosition(Vec2(sessionPtmRatio() * -0.928f, sessionPtmRatio() * -0.768f));
    addChild(wheel, -1);
    wheel = Sprite::createWithSpriteFrameName("e_van_wheel.png");
    wheel->setScale(art);
    wheel->setPosition(Vec2(sessionPtmRatio() * 0.928f, sessionPtmRatio() * -0.768f));
    addChild(wheel, -1);

    Sprite* label = Sprite::createWithSpriteFrameName("e_van_label.png");
    label->setScale(art);
    label->setPosition(Vec2(0.0f, sessionPtmRatio() * -0.832f));
    addChild(label);

    const float p0 = sessionPtmRatio();
    const float p1 = sessionPtmRatio();
    const float bottom = sessionPtmRatio() * 0.196f;
    setRefRect(Rect(p0 * -1.296f, std::fma(p1, -0.932f, -bottom), p0 * 2.592f,
                    std::fma(p1, 1.864f, bottom)));

    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(),
                         {"xMeters", "yMeters", "angle", "sleeping", "interactive"});
    setShapeCount(3);
    return true;
}

// @ios 1000ec8e0
void VanRef::setInteractive(const Value& interactive)
{
    KeyValueChange kvo(this, "interactive");
    postNotification(REF_UI_KEYS_WILL_CHANGE, this);
    Special::setInteractive(interactive);
    postNotification(REF_UI_KEYS_CHANGED, this);
}

// @ios 1000ec964
std::vector<std::string> VanRef::propertyKeysForUI()
{
    if (_interactive)
    {
        return {"x", "y", "angle", "interactive", "sleeping"};
    }
    return {"x", "y", "angle", "interactive"};
}

// @ios 1000eca40
void VanRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}
