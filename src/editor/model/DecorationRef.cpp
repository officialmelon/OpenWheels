#include "DecorationRef.h"

#include "SliderInputObject.h"
#include "platform/common/BinaryPlist.h"
#include "platform/common/IOSBundle.h"

USING_NS_CC;

namespace {

// [[shapeCountDict valueForKey:[NSString stringWithFormat:@"decoration_%i", type]] intValue]
// (missing entry -> nil -> 0).
unsigned int shapeCountForType(const ValueMap& shapeCountDict, int type)
{
    auto it = shapeCountDict.find(StringUtils::format("decoration_%i", type));
    if (it == shapeCountDict.end() || it->second.isNull())
    {
        return 0u;
    }
    return (unsigned int)it->second.asInt();
}

}  // namespace

DecorationRef::DecorationRef()
    : _sprite(nullptr)
{
}

// @ios 1000a6344
bool DecorationRef::init()
{
    if (!Special::initWithSpriteFrameName("e_1x1.png"))
    {
        return false;
    }
    setCanDragModify(false);
    setCanRotate(true);
    _decorationType = Value(0);
    refreshSprite();
    setLevelItemID(5004);
    propertyKeys().clear();
    propertyKeys().insert(propertyKeys().end(), {"xMeters", "yMeters", "angle", "decorationType",
                                                 "interactive", "fixed"});

    // Shape count per decoration body from the bundle's PhysicsEditor export.
    ValueMap shapeCounts;
    const std::string path = openwheels::iosBundlePath() + "pe_objects-hd.plist";
    if (openwheels::hasIOSBundle() && FileUtils::getInstance()->isFileExist(path))
    {
        const ValueMap file = openwheels::readPlistDict(path);  // binary or XML
        auto bodiesIt = file.find("bodies");
        if (bodiesIt != file.end() && bodiesIt->second.getType() == Value::Type::MAP)
        {
            for (const auto& body : bodiesIt->second.asValueMap())
            {
                unsigned int count = 0;
                if (body.second.getType() == Value::Type::MAP)
                {
                    const ValueMap& bodyMap = body.second.asValueMap();
                    auto fixturesIt = bodyMap.find("fixtures");
                    if (fixturesIt != bodyMap.end() &&
                        fixturesIt->second.getType() == Value::Type::VECTOR)
                    {
                        for (const auto& fixture : fixturesIt->second.asValueVector())
                        {
                            if (fixture.getType() != Value::Type::MAP)
                            {
                                continue;
                            }
                            const ValueMap& fixtureMap = fixture.asValueMap();
                            auto typeIt = fixtureMap.find("fixture_type");
                            const std::string type =
                                typeIt == fixtureMap.end() ? "" : typeIt->second.asString();
                            if (type == "POLYGON")
                            {
                                auto polygonsIt = fixtureMap.find("polygons");
                                if (polygonsIt != fixtureMap.end() &&
                                    polygonsIt->second.getType() == Value::Type::VECTOR)
                                {
                                    count += (unsigned int)polygonsIt->second.asValueVector().size();
                                }
                            }
                            else
                            {
                                count += type == "CIRCLE" ? 1u : 0u;
                            }
                        }
                    }
                }
                shapeCounts[body.first] = Value((int)count);
            }
        }
    }
    else
    {
        log("DecorationRef: pe_objects-hd.plist not found in the iOS bundle");
    }
    _shapeCountDict = shapeCounts;
    setShapeCount(shapeCountForType(_shapeCountDict, 0));
    setArtCount(0);
    return true;
}

// @ios 1000a6710
void DecorationRef::onEnter()
{
    Special::onEnter();
    setShapeCount(shapeCount());
    setArtCount(artCount());
}

// @ios 1000a6778
DecorationRef::~DecorationRef()
{
}

// @ios 1000a67d8
std::vector<std::string> DecorationRef::propertyKeysForUI()
{
    std::vector<std::string> keys = {"x", "y", "angle", "decorationType", "interactive"};
    if (_interactive)
    {
        keys.push_back("fixed");
        if (!_fixed)
        {
            keys.push_back("sleeping");
        }
    }
    return keys;
}

// @ios 1000a6894
void DecorationRef::setFixed(const Value& fixed)
{
    KeyValueChange kvo(this, "fixed");
    postNotification(REF_UI_KEYS_WILL_CHANGE, this);
    _fixed = kvcBool(fixed);
    postNotification(REF_UI_KEYS_CHANGED, this);
}

// @ios 1000a6904
void DecorationRef::setInteractive(const Value& interactive)
{
    KeyValueChange kvo(this, "interactive");
    postNotification(REF_UI_KEYS_WILL_CHANGE, this);
    const bool value = kvcBool(interactive);
    _interactive = value;
    if (!value)
    {
        setShapeCount(0);
        setArtCount(1);
    }
    else
    {
        setArtCount(0);
        setShapeCount(shapeCountForType(_shapeCountDict, kvcInt(_decorationType)));
    }
    postNotification(REF_UI_KEYS_CHANGED, this);
}

// @ios 1000a69fc
void DecorationRef::refreshShapeCount()
{
    setShapeCount(shapeCountForType(_shapeCountDict, kvcInt(_decorationType)));
}

// @ios 1000a6a78
void DecorationRef::setDecorationType(const Value& decorationType)
{
    KeyValueChange kvo(this, "decorationType");
    if (kvcInt(_decorationType) == kvcInt(decorationType))
    {
        return;
    }
    _decorationType = decorationType;
    // Also when not interactive (iOS behaviour).
    setShapeCount(shapeCountForType(_shapeCountDict, kvcInt(decorationType)));
    refreshSprite();
}

// @ios 1000a6b40
void DecorationRef::refreshSprite()
{
    if (_sprite)
    {
        removeChild(_sprite, false);
        _sprite = nullptr;
    }
    const std::string frame = StringUtils::format("e_decoration_%i.png", kvcInt(_decorationType));
    _sprite = Sprite::createWithSpriteFrameName(frame);
    double w = 0.0, h = 0.0;
    if (_sprite)
    {
        _sprite->setScale(editorArtScale());  // port: show at the iOS point size
        addChild(_sprite);
        w = (double)(_sprite->getContentSize().width * editorArtScale());
        h = (double)(_sprite->getContentSize().height * editorArtScale());
    }
    setRefRect(cg::Rect(-1.0 - w * 0.5, -1.0 - h * 0.5, w + 2.0, h + 2.0));
}

// @ios 1000a6c08
InputObject* DecorationRef::inputObjectForPropertyWithRect(const std::string& property,
                                                           const Rect& rect)
{
    if (property == "decorationType")
    {
        // max = (float)(count - 1) as a 64-bit unsigned (huge when the dictionary is empty);
        // segments = (unsigned)(count - 1).
        const unsigned long long count = (unsigned long long)_shapeCountDict.size();
        return SliderInputObject::create(rect, "TYPE", "decorationType", kvcFloat(_decorationType),
                                         0.0f, (float)(count - 1ull),
                                         (unsigned int)(count - 1ull));
    }
    return Special::inputObjectForPropertyWithRect(property, rect);
}

// @ios 1000a6d64
Value DecorationRef::decorationType()
{
    return _decorationType;
}

// ---- KVC (port) -----------------------------------------------------------------------------------

Value DecorationRef::valueForKey(const std::string& key)
{
    if (key == "decorationType") return decorationType();
    return Special::valueForKey(key);
}

void DecorationRef::setValueForKey(const Value& value, const std::string& key)
{
    if (key == "decorationType") { setDecorationType(value); return; }
    Special::setValueForKey(value, key);
}
