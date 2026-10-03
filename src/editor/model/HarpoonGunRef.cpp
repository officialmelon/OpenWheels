#include "HarpoonGunRef.h"

#include "SliderInputObject.h"
#include "SwitchInputObject.h"

#include <cmath>

USING_NS_CC;

// @ios 1000d1ba4
bool HarpoonGunRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    setShapeCount(11);
    anchor = true;
    setLevelItemID(15);
    _propertyKeys.clear();
    _propertyKeys.insert(_propertyKeys.end(),
                         {"xMeters", "yMeters", "angle", "useAnchor", "fixedTurret", "turretAngle",
                          "triggerFiring", "startDeactivated"});
    return true;
}

// @ios 1000d1cdc
void HarpoonGunRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
}

// @ios 1000d1d30
void HarpoonGunRef::setFixedTurret(bool fixedTurret)
{
    KeyValueChange kvo(this, "fixedTurret");
    if (_fixedTurret == fixedTurret)
    {
        return;
    }
    postNotification(REF_UI_KEYS_WILL_CHANGE, this);
    _fixedTurret = fixedTurret;
    postNotification(REF_UI_KEYS_CHANGED, this);
}

// @ios 1000d1db0
bool HarpoonGunRef::fixedTurret()
{
    return _fixedTurret;
}

// @ios 1000d1dc0
void HarpoonGunRef::setTriggerFiring(bool triggerFiring)
{
    KeyValueChange kvo(this, "triggerFiring");
    _triggerFiring = triggerFiring;
}

// @ios 1000d1dd0
bool HarpoonGunRef::triggerFiring()
{
    return _triggerFiring;
}

// @ios 1000d1de0
void HarpoonGunRef::setStartDeactivated(bool startDeactivated)
{
    KeyValueChange kvo(this, "startDeactivated");
    _startDeactivated = startDeactivated;
}

// @ios 1000d1df0
bool HarpoonGunRef::startDeactivated()
{
    return _startDeactivated;
}

// @ios 1000d1e00
void HarpoonGunRef::setTurretAngle(int turretAngle)
{
    KeyValueChange kvo(this, "turretAngle");
    if (_turretAngle == turretAngle)
    {
        return;
    }
    _turretAngle = turretAngle;
    // iOS messages the turret even before setUpSprites ran (nil -> no-op).
    if (_turret != nullptr)
    {
        _turret->setRotation((float)turretAngle);
    }
    updateBoundingBox();
}

// @ios 1000d1e58
void HarpoonGunRef::updateBoundingBox()
{
    // [nil boundingBox] is CGRectZero on iOS.
    Rect turretBox = _turret != nullptr ? _turret->getBoundingBox() : Rect::ZERO;
    Rect baseBox = _base != nullptr ? _base->getBoundingBox() : Rect::ZERO;
    // RE-TODO(@1000d1ec0): CGRectUnion ignores null rects; cocos2d-x unionWithRect does not.
    // Both rects are real sprite boxes once setUpSprites has run.
    setRefRect(turretBox.unionWithRect(baseBox));
}

// @ios 1000d1edc
int HarpoonGunRef::turretAngle()
{
    return _turretAngle;
}

// @ios 1000d1eec
void HarpoonGunRef::setUseAnchor(const Value& useAnchor)
{
    KeyValueChange kvo(this, "useAnchor");
    bool use = kvcBool(useAnchor);
    setShapeCount(use ? 11 : 4);
    if (anchor == use)
    {
        return;
    }
    anchor = use;
    if (rope != nullptr)
    {
        rope->setVisible(use);
    }
}

// @ios 1000d1f60
bool HarpoonGunRef::useAnchor()
{
    return anchor;
}

// @ios 1000d1f70
void HarpoonGunRef::setUpSprites()
{
    removeAllChildrenWithCleanup(false);
    const float ptm = sessionPtmRatio();

    _base = Sprite::createWithSpriteFrameName("e_harpoon_base.png");
    _base->setScale(editorArtScale());  // port: art at its iOS point size
    // textureRect height in iOS points.
    double baseHeight = _base->getTextureRect().size.height * editorArtScale();
    double baseY = std::fma(baseHeight, 0.5, 0.5) + (double)(_ptmRatio * -0.256f);
    _base->setPosition(Vec2(0.5f, (float)baseY));
    addChild(_base);

    rope = Sprite::createWithSpriteFrameName("e_harpoon_rope.png");
    rope->setScale(editorArtScale());
    rope->setAnchorPoint(Vec2::ZERO);
    rope->setVisible(anchor);
    rope->setPosition(Vec2(ptm * -0.352f, ptm * 0.288f));
    addChild(rope);

    _turret = Sprite::createWithSpriteFrameName("e_harpoon_turret.png");
    _turret->setTag(0);
    _turret->setScale(editorArtScale());
    _turret->setAnchorPoint(Vec2(0.5f, 0.0642201826f));
    _turret->setPosition(Vec2(0.0f, _ptmRatio * 0.56f));
    addChild(_turret);

    updateBoundingBox();
}

// @ios 1000d2124
void HarpoonGunRef::createRef()
{
    setUpSprites();
}

// @ios 1000d2128
InputObject* HarpoonGunRef::inputObjectForPropertyWithRect(const std::string& property,
                                                          const Rect& rect)
{
    if (property == "useAnchor")
    {
        return SwitchInputObject::create(rect, "USE ANCHOR", "useAnchor", anchor ? 1.0f : 0.0f);
    }
    if (property == "fixedTurret")
    {
        return SwitchInputObject::create(rect, "FIXED TURRET", "fixedTurret",
                                         _fixedTurret ? 1.0f : 0.0f);
    }
    if (property == "turretAngle")
    {
        return SliderInputObject::create(rect, "TURRET ANGLE", "turretAngle",
                                         (float)_turretAngle, -110.0f, 110.0f, 220);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000d235c
std::vector<std::string> HarpoonGunRef::propertyKeysForUI()
{
    if (_fixedTurret)
    {
        return {"x", "y", "angle", "useAnchor", "fixedTurret", "turretAngle", "triggerFiring",
                "startDeactivated"};
    }
    return {"x", "y", "angle", "useAnchor", "fixedTurret", "triggerFiring", "startDeactivated"};
}

Value HarpoonGunRef::valueForKey(const std::string& key)
{
    if (key == "useAnchor")
    {
        return Value(useAnchor());
    }
    if (key == "fixedTurret")
    {
        return Value(fixedTurret());
    }
    if (key == "turretAngle")
    {
        return Value(turretAngle());
    }
    if (key == "triggerFiring")
    {
        return Value(triggerFiring());
    }
    if (key == "startDeactivated")
    {
        return Value(startDeactivated());
    }
    return Special::valueForKey(key);
}

void HarpoonGunRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "useAnchor")
    {
        setUseAnchor(value);
    }
    else if (key == "fixedTurret")
    {
        setFixedTurret(kvcBool(value));
    }
    else if (key == "turretAngle")
    {
        setTurretAngle(kvcInt(value));
    }
    else if (key == "triggerFiring")
    {
        setTriggerFiring(kvcBool(value));
    }
    else if (key == "startDeactivated")
    {
        setStartDeactivated(kvcBool(value));
    }
    else
    {
        Special::setValueForKey(value, key);
    }
}
