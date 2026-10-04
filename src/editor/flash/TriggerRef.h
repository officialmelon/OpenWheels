#pragma once
// EDITOR (browser features, PC addition): a browser trigger (Flash editor RefTrigger, XML
// <triggers><t ...>). A 100 x 100 px region scaled to w x h, rotatable, with
//   b triggeredBy 1..6, t type (1 activate targets, 2 sound, 3 victory), r repeatType 1..4,
//   i repeatInterval (r > 2), d delay, sd startDisabled, s/l/p/v sound id / location / pan / volume,
// and for type 1 (or triggeredBy 4) a target list. Each target has 1..10 actions of its kind
// (FlashCatalog::targetActions) with their parameters, written as <sh|sp|g|j|t i=".."><a i p0../>
// (the v1.87 multi-action form).
//
// Targets are retained refs; a target deleted from the stage stays in the list (so undo can bring
// it back) but is skipped when the level is written.

#include <vector>

#include "Special.h"

struct TriggerAction
{
    int index = 0;
    std::vector<float> params;
};

struct TriggerTarget
{
    cocos2d::RefPtr<Special> ref;
    std::vector<TriggerAction> actions;
};

class TriggerRef : public Special
{
public:
    static const int kLevelItemID = 7000;
    static const int kMaxActions = 10;
    CREATE_FUNC(TriggerRef);
    bool init() override;

    int uid() const { return _uid; }
    // Whether targets / target actions apply (Flash: typeIndex == 1 || triggeredBy == 4 /
    // typeIndex == 1).
    bool hasTargets() const { return typeIndex == 1 || triggeredBy == 4; }
    bool hasActions() const { return typeIndex == 1; }

    std::vector<TriggerTarget>& targets() { return _targets; }
    int indexOfTarget(Special* ref) const;
    // Adds `ref` with its first action (defaults), registers undo. False when it can't be a target.
    bool addTarget(Special* ref, bool registerUndo = true);
    void removeTargetAt(int index, bool registerUndo = true);
    // Default parameters of action `actionIndex` for `ref`.
    static std::vector<float> defaultParams(Special* ref, int actionIndex);

    // Size in Flash px.
    float widthPx() const { return _widthPx; }
    float heightPx() const { return _heightPx; }

    // Special
    std::vector<std::string> propertyKeysForUI() override;
    cocos2d::Value valueForKey(const std::string& key) override;
    void setValueForKey(const cocos2d::Value& value, const std::string& key) override;
    cocos2d::ValueMap properties() override;
    void setProperties(const cocos2d::ValueMap& properties) override;
    void updateOverlayWithNode(cocos2d::DrawNode* node) override;
    void updateRefRect();
    void setNumber(int number);

    int triggeredBy = 1;
    int typeIndex = 1;
    int repeatType = 1;
    float repeatInterval = 1.0f;
    float delay = 0.0f;
    bool startDisabled = false;
    int sound = 0;            // soundlist id
    int soundLocation = 1;
    float panning = 0.0f;
    float volume = 1.0f;

protected:
    TriggerRef() = default;
    int _uid = 0;
    float _widthPx = 100.0f;
    float _heightPx = 100.0f;
    std::vector<TriggerTarget> _targets;
    cocos2d::Label* _numberLabel = nullptr;
    int _number = -1;
};
