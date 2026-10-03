#pragma once
// CharacterRef — the player start (iOS `@interface CharacterRef : Special`), levelItemID 5000.
// Always refs[0] of EditorSpriteBatchNode; EditorLayer::levelData writes it into <info x y c f h>
// (not as a special) and copySelection skips it.
//
// propertyKeys: xMeters yMeters defaultCharacter forceCharacter hideVehicle
// propertyKeysForUI: x y defaultCharacter forceCharacter hideVehicle
// The preview art is a separate node (_charNode, sprite file "e_char_<id>.png" with a
// per-character anchor) added to the ref's grandparent (the stage) at z 5000, kept at the ref's
// position. Characters come from Characters.plist ("id" per entry; iOS and Android ship the
// same list: 1,2,3,4,5,9).

#include "Special.h"

class CharacterRef : public Special
{
public:
    CREATE_FUNC(CharacterRef);

    // initWithSpriteFrameName("e_1x1.png"); canRotate 0, _dictIndex 0, _defaultCharacter 1,
    // levelItemID 5000, propertyKeys as above.
    virtual bool init() override;                                                    // @ios 1000c00e8
    int defaultCharacter();                         // character id                       // @ios 1000c0208
    // Moves _charNode too (if any), then Sprite::setPosition.
    using Special::setPosition;
    virtual void setPosition(float x, float y) override;                             // @ios 1000c0218
    // _defaultCharacter = intValue; _dictIndex = characterIndexForId(id); updateSprite.
    void setDefaultCharacter(const cocos2d::Value& defaultCharacter);                // @ios 1000c028c
    // Replaces _charNode: new Node + Sprite file "e_char_%i.png" (anchor from the iOS table,
    // index id-1), positioned at the ref, added to getParent()->getParent() at z 5000;
    // refRect = (-ax*w, -ay*h, w, h) of that sprite's textureRect.
    void updateSprite();                                                             // @ios 1000c02d8
    unsigned int characterIdForIndex(unsigned int index);   // list[index]["id"]      // @ios 1000c0408
    unsigned int characterIndexForId(unsigned int characterId);  // 0 if not found    // @ios 1000c0448
    int forceCharacter();                                                            // @ios 1000c04d0
    void setForceCharacter(const cocos2d::Value& forceCharacter);   // boolValue      // @ios 1000c04e0
    virtual void setRotation(float rotation) override;  // no-op                      // @ios 1000c0510
    virtual std::vector<std::string> propertyKeysForUI() override;                   // @ios 1000c0514
    // 1-based slider value: _dictIndex = v - 1; _defaultCharacter = characterIdForIndex;
    // updateSprite.
    void setDictIndex(unsigned int dictIndex);                                       // @ios 1000c056c
    unsigned int dictIndex();                       // _dictIndex + 1                     // @ios 1000c05ac
    // defaultCharacter: SliderInputObject DEFAULT CHARACTER, property "dictIndex", initial
    // _dictIndex+1, min 1, max count, count-1 segments; forceCharacter / hideVehicle:
    // SwitchInputObject FORCE CHARACTER / HIDE VEHICLE; else Special's.
    virtual InputObject* inputObjectForPropertyWithRect(const std::string& property,
                                                        const cocos2d::Rect& rect) override;  // @ios 1000c05c0
    bool hideVehicle();                                                              // @ios 1000c0800
    void setHideVehicle(bool hideVehicle);                                           // @ios 1000c0810

    // port: KVC — defaultCharacter forceCharacter hideVehicle dictIndex (+ Special's).
    virtual cocos2d::Value valueForKey(const std::string& key) override;
    virtual void setValueForKey(const cocos2d::Value& value, const std::string& key) override;

protected:
    CharacterRef();
    virtual ~CharacterRef();

    // iOS ivars (CharacterRef : Special, instanceSize 0x279)
    unsigned int _dictIndex;                // _dictIndex        0-based list index
    unsigned int _defaultCharacter;         // _defaultCharacter character id
    bool _forceCharacter;                   // _forceCharacter
    cocos2d::Node* _charNode;               // _charNode         CCNode (strong)
    bool _hideVehicle;                      // _hideVehicle
};
