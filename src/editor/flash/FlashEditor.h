#pragma once
// EDITOR (browser features, PC addition): shared helpers of the browser-feature editor refs
// (FlashSpecialRef, PolygonRefShape, TriggerRef, JointRef, GroupRef) and the browser-format level
// reader/writer.
//
// Units: refs live in the iOS editor's stage space (stage units, y up, ptm = sessionPtmRatio()
// stage units per metre, 320 x 160 m). The browser format is Flash editor pixels (62.5 px/m, y
// down, 20000 x 10000 px). Both measure rotation in degrees clockwise (cocos2d rotation and
// Flash's rotation), so angles map 1:1.
//
// Trigger actions are edited through KVC keys on any ref (so EditParametersView's undo and KVO
// work unchanged):
//   "act:<trigger uid>:<target>:<action>"            the action choice (value = list index)
//   "act:<trigger uid>:<target>:<action>:<param>"    one action parameter
// and rows with buttons ("actadd:", "actdel:", "unlink:", "link:") that act directly.

#include <string>
#include <vector>

#include "cocos2d.h"
#include "FlashCatalog.h"

class EditorSpriteBatchNode;
class EditorUndoManager;
class InputObject;
class Special;
class TriggerRef;

namespace flashed {

// ---- editor context (set by EditorLayer while it lives) ---------------------------------------
void setStage(EditorSpriteBatchNode* sbn);
EditorSpriteBatchNode* stage();
EditorUndoManager* undoManager();
// Every TriggerRef on the stage, in stage order (= XML order).
std::vector<TriggerRef*> stageTriggers();
// 1-based number shown on a trigger (its XML index + 1).
int triggerNumber(TriggerRef* trigger);
TriggerRef* triggerWithUid(int uid);
bool onStage(Special* ref);

// Modifier keys, tracked by EditorLayer's keyboard listener (PC).
bool shiftDown();
void setShiftDown(bool down);

// ---- units ------------------------------------------------------------------------------------
float ptm();                                    // stage units per metre
cocos2d::Vec2 stageToPx(const cocos2d::Vec2& p);
cocos2d::Vec2 pxToStage(float xPx, float yPx);
inline float pxToStageLength(float px) { return px / kPxPerMetre * ptm(); }
inline float stageToPxLength(float s) { return s / ptm() * kPxPerMetre; }
// Rotation in -180..180 (Flash RefSprite angle).
float normalizedAngle(float degrees);

// ---- trigger targets / joints -----------------------------------------------------------------
// The kind and special id a ref has as a trigger target; false when it can't be one.
bool targetKind(Special* ref, TargetKind* kind, int* specialType);
const std::vector<ActionInfo>* actionsFor(Special* ref);
// Whether a joint can attach to this ref (Flash RefSprite.joinable).
bool joinable(Special* ref);
// What a tap on `ref` selects / links: its group when it is grouped, else itself.
Special* unitOf(Special* ref);
// Display name ("rectangle", "trigger 3", "group", ...).
std::string displayName(Special* ref);

// KVC hooks called by Special (keys starting with "act:"); false when the key is not one.
bool valueForActionKey(Special* ref, const std::string& key, cocos2d::Value* out);
bool setValueForActionKey(Special* ref, const std::string& key, const cocos2d::Value& value);
// The trigger-action rows a target shows in its parameters panel (Flash addTriggerProperties).
std::vector<std::string> targetUIKeys(Special* ref);
// Rows of a trigger's own panel for its targets.
std::vector<std::string> triggerTargetUIKeys(TriggerRef* trigger);
// Inputs for the keys above (nullptr when not an action key).
InputObject* actionInputObject(Special* ref, const std::string& key, const cocos2d::Rect& rect);

// ---- property panel helpers ---------------------------------------------------------------------
// An input for a catalog attribute (FlashCatalog Attr) editing `property` of the selected ref.
InputObject* makeInput(const Attr& attr, const std::string& property, float value, const cocos2d::Rect& rect);
// Rebuilds the parameters panel next frame (keys appear / disappear), like RefShape's
// ref_ui_keys_will_change / ref_ui_keys_changed pair, but safe to call from an input callback.
void uiKeysChangedLater(Special* ref);
// Rebuilds the panel next frame for whatever is selected.
void refreshPanelLater();

// ---- art ------------------------------------------------------------------------------------------
// generated/flash/<name>.png as a sprite anchored on the Flash registration point and scaled to
// stage units (1 Flash px = pxToStageLength(1)). nullptr when the art was not generated.
cocos2d::Sprite* flashArtSprite(const std::string& name);
// The same image for a UI icon (no scaling), nullptr if missing.
cocos2d::Sprite* flashIconSprite(const std::string& name);
// Text box fonts (generated/flash/fonts/*.ttf), "" when missing.
std::string fontFile(int font);

// Monotonic ids for TriggerRef (stable through undo) and polygon vertex lists.
int nextUid();

// Shared colours of the editor overlays.
extern const cocos2d::Color4F kLinkColor;      // trigger -> target lines
extern const cocos2d::Color4F kJointColor;     // joint arms (Flash 0xff6600)
extern const cocos2d::Color4F kGroupColor;     // group outline

}  // namespace flashed
