#pragma once
// Special — base class of every level-editor object ("ref") in the iOS 1.2.7 editor
// (`@interface Special : CCSprite`). Ported to cocos2d-x 3.17.2 per docs/EDITOR_PORT.md.
//
// A ref is a placeholder sprite inside EditorSpriteBatchNode. It carries the item's editable
// properties and knows how to (de)serialize them to the level XML and how to describe its
// property-editing UI. Concrete refs: RefShape (+Circle/Rectangle/TriangleRefShape),
// CharacterRef, DecorationRef (E1) and the ~19 per-item *Ref classes (E2).
//
// ---------------------------------------------------------------------------------------------
// Units
//   Positions/sizes are in editor points (the coordinate space of EditorSpriteBatchNode). The
//   *Meters accessors divide/multiply by _ptmRatio, which iOS copies from
//   Session.sharedSession.ptmRatio at init time; the editor runs in SessionMode 2 where iOS uses
//   72 (iPad) / 40 (iPhone). The port has no shared Session in the editor, so EditorLayer (E3)
//   sets Special::setSessionPtmRatio() before creating refs (default 72 = iPad, matching the
//   -ipad/-ipadhd editor art laid out in iPad points). The XML stores meters, so the ratio only
//   affects on-screen scale, not the file.
//
// ---------------------------------------------------------------------------------------------
// Serialization contract (used by E3's EditorLayer::levelData / addSingleItem / copy & paste)
//   * levelItemID(): the XML type. Specials are written as  <sp t="levelItemID" p0=.. p1=.. />;
//     RefShapes (levelItemID 6000/6001/6002) as  <sh t="levelItemID-6000" i="interactive" .. />.
//   * propertyKeys(): ordered KVC keys; key i is written as attribute p<i>.
//   * valueForKey(key).asFloat(): the number to write. -[EditorLayer levelData] writes
//     `p%i="%i"` with (int)v when fmodf(v, 1) == 0, else `p%i="%.02f"`.
//   * properties(): the copy/paste form: {"t": levelItemID (int), "p<i>": string}, where the
//     string is "%.02f" with the last 3 chars dropped if it contains ".00" ("5.00" -> "5").
//     (RefShape adds no "i", so pasted shapes come back interactive — iOS behaviour.)
//   * setProperties(dict): for i = 0.. while dict has "p<i>":
//       setValueForKey(Value(dict["p<i>"].asFloat()), propertyKeys()[i]).
//     Values in dict may be strings (XML attributes) or numbers. RefShape also reads "i".
//   * createRef(): instance hook (iOS `-create`) called after setProperties()/positioning,
//     before the ref is added to EditorSpriteBatchNode. Default: nothing.
//
// KVC (NSKeyValueCoding) — the port's replacement for -valueForKey:/-setValue:forKey:
//   valueForKey()/setValueForKey() dispatch on the key name to the getter/setter of that name,
//   exactly like KVC: a subclass handles its own keys and chains to its base for the rest.
//   Boxing follows NSNumber: getters return Value(float|int|bool); setters that take an
//   NSNumber* on iOS take `const cocos2d::Value&` here, scalar setters take the scalar converted
//   with the kvc* helpers below (floatValue / intValue / unsignedIntValue / boolValue).
//   An unknown key is NSUnknownKeyException on iOS; here it logs and returns Value::Null.
//
// KVO (NSKeyValueObserving) — EditParametersView (E4) observes every propertyKeysForUI() key
//   of the selected refs (options = New) to refresh its inputs when a ref changes (dragging
//   fires "x"/"y", rotating fires "angle", ...). iOS fires automatic KVO whenever the setter
//   `set<Key>:` runs, once per outermost call. Port rule: EVERY setter whose iOS selector is
//   `set<Key>:` for a KVC key starts with
//       KeyValueChange kvo(this, "<key>");
//   which notifies observers of <key> with valueForKey(<key>) when the outermost such scope for
//   that key ends (an override that chains to the base setter therefore notifies once, after
//   its own code — as on iOS).
//
// Property-editing UI contract (E4)
//   propertyKeysForUI(): ordered keys EditParametersView shows (default: propertyKeys()).
//   inputObjectForPropertyWithRect(key, frame): a new autoreleased InputObject (E4 class family:
//   InputObject, SliderInputObject, SwitchInputObject, ColorInputObject) for that key, or
//   nullptr when the ref has no input for it (EditParametersView then skips the key). The
//   InputObject's `property` is the KVC key it writes back through setValueForKey(); it may
//   differ from the UI key (CharacterRef: UI "defaultCharacter" -> property "dictIndex").
//   Labels are passed as Localizable.strings KEYS ("ANGLE", "r"); InputObject localizes them
//   (E4 contract; "" = nil label). See docs/editor/E1.md for the exact constructors used.
//
// Notifications: uikit::NotificationCenter (E4; EventCustom on the Director's dispatcher,
//   userData = the posting object or nullptr): see the constants below.
//
// Geometry: refRect / refBoundingBox / startPos are CoreGraphics doubles (cg::Rect/cg::Point,
//   E3's EditorGeometry.h), as on iOS (CGFloat = double on arm64).
//
// Art scale (port, decided by E3): stage space is measured in editor-atlas art units (texture
//   pixels / contentScaleFactor), i.e. uniformly scaled iPad points: 1 unit =
//   EditorLayer::stageUnitInPoints() points, and sessionPtmRatio() = 72 / stageUnitInPoints().
//   All iOS ptm-based geometry stays consistent and every sprite (own-frame art or child art)
//   shows at its iOS size with scale 1, so editorArtScale() is 1 (kept so the call sites document
//   where iOS relied on content scale). The ref node itself keeps scale 1.
// ---------------------------------------------------------------------------------------------

#include "cocos2d.h"

#include "EditorGeometry.h"

#include <functional>
#include <string>
#include <vector>

class InputObject;  // E4 (src/editor/ui/InputObject.h)
class GroupRef;     // EDITOR (browser features, PC addition): src/editor/flash/GroupRef.h

// Construction: concrete refs use CREATE_FUNC(T) -> `T::create()` = `[[[T alloc] init]
// autorelease]` (their `bool init() override` is the iOS -init). The iOS instance method
// `-create` is therefore named createRef() here (agreed with E2).

class Special : public cocos2d::Sprite
{
public:
    // ---- notification names (exact iOS strings) -------------------------------------------
    static const char* const SHAPE_COUNT_UPDATE;       // "SHAPE_COUNT_UPDATE", object nullptr
    static const char* const ART_COUNT_UPDATE;         // "ART_COUNT_UPDATE", object nullptr
    static const char* const REF_UI_KEYS_WILL_CHANGE;  // "ref_ui_keys_will_change", object = ref
    static const char* const REF_UI_KEYS_CHANGED;      // "ref_ui_keys_changed", object = ref
    // [[NSNotificationCenter defaultCenter] postNotificationName:name object:object]
    static void postNotification(const std::string& name, void* object);

    // Port: iPad points per (texture pixel / contentScaleFactor) unit, i.e.
    // contentScaleFactor / EditorAssets::pixelsPerPoint().
    static float editorArtScale();

    // ---- editor ptm ratio (port: replaces Session.sharedSession.ptmRatio in SessionMode 2) ---
    static void setSessionPtmRatio(float ptmRatio);
    static float sessionPtmRatio();

    // ---- KVO -------------------------------------------------------------------------------
    using KeyValueObserver =
        std::function<void(const std::string& keyPath, Special* object, const cocos2d::Value& newValue)>;
    // -addObserver:forKeyPath:options:NSKeyValueObservingOptionNew context:
    void addObserver(const void* observer, const std::string& keyPath, const KeyValueObserver& callback);
    // -removeObserver:forKeyPath:
    void removeObserver(const void* observer, const std::string& keyPath);
    // Fires the observers of `key` with valueForKey(key) (manual -didChangeValueForKey:).
    void didChangeValueForKey(const std::string& key);

    // ---- initialisation (CCSprite overrides) ----------------------------------------------
    // snaps = 1, canDragModify = 0, locked = 0, canRotate = 1, interactive = 1,
    // ptmRatio = sessionPtmRatio(). (Calls initWithSpriteFrame via the base.)
    virtual bool initWithSpriteFrameName(const std::string& spriteFrameName) override;  // @ios 1000b4fc8
    // propertyKeys = [xMeters, yMeters, angle, fixed, sleeping]; refRect.size = textureRect.size
    // (refRect.origin stays 0,0).
    virtual bool initWithSpriteFrame(cocos2d::SpriteFrame* spriteFrame) override;      // @ios 1000b50a4

    // Settings.nameForLevelItem(levelItemID): localized item name (EditParametersView title).
    std::string name();                                                                  // @ios 1000b506c

    // ---- property-editing UI ---------------------------------------------------------------
    // Special handles: "x"/"y" (InputObject, label X/Y, initial = cached _x/_y, display field),
    // "angle" (InputObject, label ANGLE, initial = rotation, display field, min 0 max 360,
    // isAngleValue), "fixed"/"sleeping"/"interactive" (SwitchInputObject, label FIXED/SLEEPING/
    // INTERACTIVE, initial 1/0); anything else -> nullptr.
    virtual InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                        const cocos2d::Rect& rect);      // @ios 1000b5154
    // Default: propertyKeys().
    virtual std::vector<std::string> propertyKeysForUI();                                // @ios 1000b5c38

    // ---- node lifecycle --------------------------------------------------------------------
    // Caches position into _x/_y, then CCNode onEnter.
    virtual void onEnter() override;                                                     // @ios 1000b54e0
    virtual ~Special();                                                                  // @ios 1000b554c (dealloc)

    // Hit test: point in the PARENT's space -> world -> node space; CGRectContainsPoint(refRect)
    // (half-open: minX <= x < maxX, minY <= y < maxY).
    bool containsPoint(const cg::Point& point);                                          // @ios 1000b559c

    // ---- position / rotation (KVC keys x, y, xMeters, yMeters, angle) ----------------------
    void setX(float x, float y);                    // setX:@(x) then setY:@(y)          // @ios 1000b55fc
    virtual void setX(const cocos2d::Value& x);     // _x = x; setPosition(_x, _y)        // @ios 1000b5650
    virtual void setY(const cocos2d::Value& y);     // _y = y; setPosition(_x, _y)        // @ios 1000b5698
    float xMeters();                                // (float)(position.x / ptmRatio)     // @ios 1000b56e4
    virtual void setXMeters(const cocos2d::Value& xMeters);  // setX:@(ptmRatio * v)      // @ios 1000b571c
    float yMeters();                                                                     // @ios 1000b5768
    virtual void setYMeters(const cocos2d::Value& yMeters);                              // @ios 1000b57a0
    virtual void setAngle(const cocos2d::Value& angle);      // setRotation(v) (virtual)  // @ios 1000b57ec
    float x();                                      // position.x (not the cached _x)     // @ios 1000b58d4
    float y();                                      // position.y                         // @ios 1000b58ec
    float angle();                                  // rotation                           // @ios 1000b5904

    // ---- flags (KVC keys fixed, sleeping, interactive) -------------------------------------
    bool fixed();                                                                        // @ios 1000b5814
    virtual void setFixed(const cocos2d::Value& fixed);              // boolValue         // @ios 1000b5824
    bool sleeping();                                                                     // @ios 1000b5854
    virtual void setSleeping(const cocos2d::Value& sleeping);                            // @ios 1000b5864
    bool interactive();                                                                  // @ios 1000b5894
    virtual void setInteractive(const cocos2d::Value& interactive);                      // @ios 1000b58a4

    // ---- size (KVC keys width, height): Special has none; RefShape/IBeamRef/LogRef override -
    virtual float height();                         // 0                                  // @ios 1000b5908
    virtual void setHeight(float height);           // no-op (iOS takes an id, ignored)   // @ios 1000b5910
    virtual float width();                          // 0                                  // @ios 1000b5914
    virtual void setWidth(float width);             // no-op                              // @ios 1000b591c

    // ---- shape/art budget (EditorLayer sums these against the level limits) ---------------
    void setShapeCount(unsigned int shapeCount);    // + post SHAPE_COUNT_UPDATE           // @ios 1000b5920
    void setArtCount(unsigned int artCount);        // + post ART_COUNT_UPDATE             // @ios 1000b5954
    unsigned int artCount();                                                             // @ios 1000b6004
    unsigned int shapeCount();                                                           // @ios 1000b6014

    // ---- selection geometry ----------------------------------------------------------------
    // refRect in node space mapped by nodeToParentTransform (axis-aligned bounds in the
    // parent's space). CircleRefShape overrides. Ignores _refBoundingBox.
    virtual cg::Rect refBoundingBox();                                                   // @ios 1000b5988
    void setRefBoundingBox(const cg::Rect& refBoundingBox);                              // @ios 1000b5f5c
    cg::Rect refRect();                             // hit/selection rect in node space   // @ios 1000b5f04
    void setRefRect(const cg::Rect& refRect);                                            // @ios 1000b5f1c
    void setRefRect(const cocos2d::Rect& refRect);  // port convenience (float rect)
    cg::Point startPos();                           // drag start (EditorSpriteBatchNode)  // @ios 1000b5f34
    void setStartPos(const cg::Point& startPos);                                         // @ios 1000b5f48
    // cocos2d-iphone's nodeToParentTransform with `position` substituted for the node's own
    // position (no callers in 1.2.7; ported for completeness).
    cocos2d::AffineTransform nodeToParentTransformWithPosition(const cocos2d::Vec2& position);  // @ios 1000b5c44

    // ---- serialization ---------------------------------------------------------------------
    virtual void setProperties(const cocos2d::ValueMap& properties);                     // @ios 1000b59e4
    virtual cocos2d::ValueMap properties();                                              // @ios 1000b5ad0
    std::vector<std::string>& propertyKeys();       // mutable, like the NSMutableArray   // @ios 1000b5f74
    int levelItemID();                                                                   // @ios 1000b5fe4
    void setLevelItemID(int levelItemID);                                                // @ios 1000b5ff4

    // ---- KVC -------------------------------------------------------------------------------
    // Special's keys: x, y, xMeters, yMeters, angle, fixed, sleeping, interactive, width, height.
    virtual cocos2d::Value valueForKey(const std::string& key);
    virtual void setValueForKey(const cocos2d::Value& value, const std::string& key);

    // ---- hooks -----------------------------------------------------------------------------
    virtual void createRef();                       // iOS -create; no-op                 // @ios 1000b5c3c
    using cocos2d::Sprite::update;
    virtual void update();                          // no-op                              // @ios 1000b5c40
    // EditorSpriteBatchNode::update clears its DrawNode, then calls this on every ref that
    // responds to -updateDrawingWithNode: (RefShape family, SlowMotionPanelRef). Default:
    // nothing (port: Special itself does not implement it on iOS; calling the no-op is
    // equivalent to the respondsToSelector: check).
    virtual void updateDrawingWithNode(cocos2d::DrawNode* node);

    // ---- editor flags ----------------------------------------------------------------------
    bool canDragModify();                                                                // @ios 1000b5f84
    void setCanDragModify(bool canDragModify);                                           // @ios 1000b5f94
    bool canRotate();                                                                    // @ios 1000b5fa4
    void setCanRotate(bool canRotate);                                                   // @ios 1000b5fb4
    bool locked();                                                                       // @ios 1000b5fc4
    void setLocked(bool locked);                                                         // @ios 1000b5fd4

    // ---- EDITOR (browser features, PC addition): see src/editor/flash/ --------------------
    // The browser-style group this ref belongs to (not retained; GroupRef keeps its members).
    GroupRef* group() const { return _group; }
    void setGroup(GroupRef* group) { _group = group; }
    // Drops every KVO registration of `observer` (any key path).
    void removeAllObservers(const void* observer);
    // Overlay drawn above all items each frame (trigger regions, links, joint arms, groups).
    virtual void updateOverlayWithNode(cocos2d::DrawNode* node) {}
    // Called by EditorSpriteBatchNode after a drag / nudge of this ref ended.
    virtual void didMove() {}

protected:
    Special();

    // NSNumber accessors for KVC-boxed values (NSNumber semantics on arm64).
    static float kvcFloat(const cocos2d::Value& v);          // -floatValue
    static int kvcInt(const cocos2d::Value& v);              // -intValue (truncation)
    static unsigned int kvcUnsigned(const cocos2d::Value& v);  // -unsignedIntValue (fcvtzu: <0 -> 0)
    static bool kvcBool(const cocos2d::Value& v);            // -boolValue (non-zero)

    // RAII automatic-KVO scope; see the header comment.
    class KeyValueChange
    {
    public:
        KeyValueChange(Special* object, const char* key);
        ~KeyValueChange();
    private:
        Special* _object;
        const char* _key;
        bool _outermost;
    };

    // iOS ivars (ObjC metadata, Special : CCSprite, instanceSize 0x2d0). Leading underscores
    // added where the iOS ivar name equals its accessor's name (x, y, fixed, ...).
    float _x;                                // +0x254 x            cached position.x (onEnter, setX:)
    float _y;                                // +0x258 y            cached position.y
    float _ptmRatio;                         // +0x25c ptmRatio     from Session at init
    bool _fixed;                             // +0x260 fixed
    bool _sleeping;                          // +0x261 sleeping
    bool _snaps;                             // +0x262 snaps        set 1 in init, never read
    bool _interactive;                       // +0x263 interactive
    bool _canDragModify;                     // +0x264 _canDragModify
    bool _canRotate;                         // +0x265 _canRotate
    bool _locked;                            // +0x266 _locked
    int _levelItemID;                        // +0x268 _levelItemID
    unsigned int _artCount;                  // +0x26c _artCount
    unsigned int _shapeCount;                // +0x270 _shapeCount
    std::vector<std::string> _propertyKeys;  // +0x278 _propertyKeys    NSMutableArray<NSString>
    cg::Point _startPos;                     // +0x280 _startPos        CGPoint
    cg::Rect _refRect;                       // +0x290 _refRect         CGRect
    cg::Rect _refBoundingBox;                // +0x2b0 _refBoundingBox  CGRect

private:
    struct Observation
    {
        const void* observer;
        std::string keyPath;
        KeyValueObserver callback;
    };
    std::vector<Observation> _observations;  // port: KVO registry
    std::vector<std::string> _kvoChanging;   // port: keys with an open KeyValueChange scope
    GroupRef* _group = nullptr;        // EDITOR (browser features, PC addition)
};
