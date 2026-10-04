// EDITOR (browser features, PC addition): see FlashEditorHooks.h.
#include "FlashEditorHooks.h"

#include "cocos2d.h"
#include "EditorLayer.h"
#include "online/OnlinePlay.h"

USING_NS_CC;

namespace flashed {

void openLevelInEditor(const std::string& xml, const std::string& name)
{
    Scene* scene = EditorLayer::createSceneWithXML(xml, name);
    if (Director::getInstance()->getRunningScene())
        Director::getInstance()->replaceScene(scene);
    else
        Director::getInstance()->runWithScene(scene);
}

namespace {
// The online browser's EDIT button (src/online/OnlineLevelBrowser.cpp).
struct RegisterOnlineHook
{
    RegisterOnlineHook()
    {
        online::setOpenInEditorHandler([](const std::string& xml, const online::OnlineLevelInfo& level) {
            openLevelInEditor(xml, level.name);
        });
    }
} s_registerOnlineHook;
}  // namespace

}  // namespace flashed
