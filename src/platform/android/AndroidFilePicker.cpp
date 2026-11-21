// EDITOR: Android's system file picker for "import levels" (see platform/common/FilePicker.h and
// AppActivity.pickLevelFiles).
#include <jni.h>

#include <mutex>

#include "cocos2d.h"
#include "platform/android/jni/JniHelper.h"
#include "platform/common/FilePicker.h"

namespace openwheels {
namespace filepicker {
static std::mutex g_lock;
static std::function<void(int, int)> g_done;

bool available() { return true; }

void pickLevelFiles(const std::string& destDir, std::function<void(int, int)> done)
{
    {
        std::lock_guard<std::mutex> guard(g_lock);
        g_done = std::move(done);
    }
    cocos2d::JniHelper::callStaticVoidMethod("org/openwheels/game/AppActivity", "pickLevelFiles", destDir);
}

}  // namespace filepicker
}  // namespace openwheels

namespace openwheels {
namespace filepicker {

// GL thread.
void deliver(int copied, int skipped)
{
    std::function<void(int, int)> done;
    {
        std::lock_guard<std::mutex> guard(g_lock);
        done = std::move(g_done);
        g_done = nullptr;
    }
    if (done) done(copied, skipped);
}

}  // namespace filepicker
}  // namespace openwheels

extern "C" {

JNIEXPORT void JNICALL Java_org_openwheels_game_AppActivity_nativeLevelsPicked(JNIEnv*, jclass, jint copied,
                                                                                 jint skipped)
{
    // The callback touches the scene: run it on the cocos2d-x thread whichever thread called.
    cocos2d::Director::getInstance()->getScheduler()->performFunctionInCocosThread(
        [copied, skipped]() { openwheels::filepicker::deliver(copied, skipped); });
}

}
