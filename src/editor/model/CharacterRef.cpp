#include "CharacterRef.h"

#include "Settings.h"
#include "SliderInputObject.h"
#include "SwitchInputObject.h"
#include "platform/common/EditorAssets.h"
#include "platform/common/AppleImage.h"
#include "platform/common/IOSBundle.h"

USING_NS_CC;

namespace {

// Anchor point of the "e_char_<id>" preview art, indexed by character id - 1 (iOS __const
// @10113b758, CGPoint doubles holding float values).
const double kCharacterAnchors[][2] = {
    {0.3815822899341583, 0.6378175020217896},   // 1
    {0.24773959815502167, 0.7961613535881042},  // 2
    {0.4757375717163086, 0.7751890420913696},   // 3
    {0.202062726020813, 0.6577166318893433},    // 4
    {0.3897757828235626, 0.7408882975578308},   // 5
    {0.33511170744895935, 0.665166974067688},   // 6
    {0.456049382686615, 0.6972619891166687},    // 7
    {0.32801103591918945, 0.9636803865432739},  // 8
    {0.4059620499610901, 0.7509888410568237},   // 9
    {0.6240715384483337, 0.7320181131362915},   // 10
    {0.6067500114440918, 0.572317898273468},    // 11
};
const unsigned int kCharacterAnchorCount = sizeof(kCharacterAnchors) / sizeof(kCharacterAnchors[0]);

// iOS Session.sharedSession.characterDataList (Characters.plist).
ValueVector characterDataList()
{
    return Settings::getInstance()->getAllCharactersData();
}

int characterId(const Value& entry)
{
    if (entry.getType() != Value::Type::MAP)
    {
        return 0;
    }
    const ValueMap& map = entry.asValueMap();
    auto it = map.find("id");
    return it == map.end() ? 0 : it->second.asInt();
}

}  // namespace

CharacterRef::CharacterRef()
    : _dictIndex(0)
    , _defaultCharacter(0)
    , _forceCharacter(false)
    , _charNode(nullptr)
    , _hideVehicle(false)
{
}

CharacterRef::~CharacterRef()
{
    // iOS dealloc leaves _charNode in the stage; the strong reference is dropped.
    CC_SAFE_RELEASE(_charNode);
}

// @ios 1000c00e8
bool CharacterRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    setCanRotate(false);
    _dictIndex = 0;
    _defaultCharacter = 1;
    setLevelItemID(5000);
    propertyKeys().clear();
    propertyKeys().insert(propertyKeys().end(),
                          {"xMeters", "yMeters", "defaultCharacter", "forceCharacter",
                           "hideVehicle"});
    return true;
}

// @ios 1000c0208
int CharacterRef::defaultCharacter()
{
    return (int)_defaultCharacter;
}

// @ios 1000c0218
void CharacterRef::setPosition(float x, float y)
{
    if (_charNode)
    {
        _charNode->setPosition(x, y);
    }
    Special::setPosition(x, y);
}

// @ios 1000c028c
void CharacterRef::setDefaultCharacter(const Value& defaultCharacter)
{
    KeyValueChange kvo(this, "defaultCharacter");
    const unsigned int id = (unsigned int)kvcInt(defaultCharacter);
    _defaultCharacter = id;
    _dictIndex = characterIndexForId(id);
    updateSprite();
}

// @ios 1000c02d8
void CharacterRef::updateSprite()
{
    if (_charNode)
    {
        _charNode->removeFromParentAndCleanup(false);
    }
    CC_SAFE_RELEASE(_charNode);
    _charNode = Node::create();
    CC_SAFE_RETAIN(_charNode);

    const unsigned int index = _defaultCharacter - 1;
    double anchorX = 0.0, anchorY = 0.0;
    if (index < kCharacterAnchorCount)
    {
        anchorX = kCharacterAnchors[index][0];
        anchorY = kCharacterAnchors[index][1];
    }
    else
    {
        // RE-TODO(@1000c02d8): iOS reads past the 11-entry table for other ids.
        log("CharacterRef::updateSprite: no anchor for character %u", _defaultCharacter);
    }

    // [CCSprite spriteWithFile:@"e_char_%i.png"] from the iOS bundle (device suffix applied by
    // cocos2d-iphone; here EditorAssets' -ipad / -ipadhd).
    const std::string file = openwheels::iosBundlePath() +
                             StringUtils::format("e_char_%i", (int)_defaultCharacter) +
                             EditorAssets::suffix() + ".png";
    Texture2D* texture = openwheels::addTexture(file);  // the e_char PNGs are CgBI
    Sprite* sprite = texture ? Sprite::createWithTexture(texture) : nullptr;
    if (sprite)
    {
        sprite->setAnchorPoint(Vec2((float)anchorX, (float)anchorY));
        sprite->setScale(editorArtScale());  // port: show at the iOS point size
        _charNode->addChild(sprite);
    }
    _charNode->setPosition(getPosition());
    Node* parent = getParent();
    Node* stage = parent ? parent->getParent() : nullptr;
    if (stage)
    {
        stage->addChild(_charNode, 5000);
    }

    // textureRect in iOS points (nil sprite -> CGRectZero).
    const float scale = editorArtScale();
    const float w = sprite ? sprite->getTextureRect().size.width * scale : 0.0f;
    const float h = sprite ? sprite->getTextureRect().size.height * scale : 0.0f;
    setRefRect(cg::Rect(anchorX * (double)-w, anchorY * (double)-h, (double)w, (double)h));
}

// @ios 1000c0408
unsigned int CharacterRef::characterIdForIndex(unsigned int index)
{
    const ValueVector list = characterDataList();
    if (index >= list.size())
    {
        // iOS: NSRangeException. Port: log, 0.
        log("CharacterRef::characterIdForIndex: index %u out of range", index);
        return 0;
    }
    return (unsigned int)characterId(list[index]);
}

// @ios 1000c0448
unsigned int CharacterRef::characterIndexForId(unsigned int characterIdToFind)
{
    const ValueVector list = characterDataList();
    for (unsigned int i = 0; i < list.size(); ++i)
    {
        if ((unsigned int)characterId(list[i]) == characterIdToFind)
        {
            return i;
        }
    }
    return 0;
}

// @ios 1000c04d0
int CharacterRef::forceCharacter()
{
    return _forceCharacter ? 1 : 0;
}

// @ios 1000c04e0
void CharacterRef::setForceCharacter(const Value& forceCharacter)
{
    KeyValueChange kvo(this, "forceCharacter");
    _forceCharacter = kvcBool(forceCharacter);
}

// @ios 1000c0510
void CharacterRef::setRotation(float rotation)
{
    KeyValueChange kvo(this, "rotation");
}

// @ios 1000c0514
std::vector<std::string> CharacterRef::propertyKeysForUI()
{
    return {"x", "y", "defaultCharacter", "forceCharacter", "hideVehicle"};
}

// @ios 1000c056c
void CharacterRef::setDictIndex(unsigned int dictIndex)
{
    KeyValueChange kvo(this, "dictIndex");
    _dictIndex = dictIndex - 1;
    _defaultCharacter = characterIdForIndex(dictIndex - 1);
    updateSprite();
}

// @ios 1000c05ac
unsigned int CharacterRef::dictIndex()
{
    return _dictIndex + 1;
}

// @ios 1000c05c0
InputObject* CharacterRef::inputObjectForPropertyWithRect(const std::string& property,
                                                          const Rect& rect)
{
    if (property == "defaultCharacter")
    {
        const unsigned int count = (unsigned int)characterDataList().size();
        return SliderInputObject::create(rect, "DEFAULT CHARACTER", "dictIndex",
                                         (float)(_dictIndex + 1), 1.0f, (float)count, count - 1);
    }
    if (property == "forceCharacter")
    {
        return SwitchInputObject::create(rect, "FORCE CHARACTER", "forceCharacter",
                                         _forceCharacter ? 1.0f : 0.0f);
    }
    if (property == "hideVehicle")
    {
        return SwitchInputObject::create(rect, "HIDE VEHICLE", "hideVehicle",
                                         _hideVehicle ? 1.0f : 0.0f);
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000c0800
bool CharacterRef::hideVehicle()
{
    return _hideVehicle;
}

// @ios 1000c0810
void CharacterRef::setHideVehicle(bool hideVehicle)
{
    KeyValueChange kvo(this, "hideVehicle");
    _hideVehicle = hideVehicle;
}

// ---- KVC (port) -----------------------------------------------------------------------------------

Value CharacterRef::valueForKey(const std::string& key)
{
    if (key == "defaultCharacter") return Value(defaultCharacter());
    if (key == "forceCharacter") return Value(forceCharacter());
    if (key == "hideVehicle") return Value(hideVehicle());
    if (key == "dictIndex") return Value(dictIndex());
    return Special::valueForKey(key);
}

void CharacterRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "defaultCharacter") { setDefaultCharacter(value); return; }
    if (key == "forceCharacter") { setForceCharacter(value); return; }
    if (key == "hideVehicle") { setHideVehicle(kvcBool(value)); return; }
    if (key == "dictIndex") { setDictIndex(kvcUnsigned(value)); return; }
    Special::setValueForKey(value, key);
}
