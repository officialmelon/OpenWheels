// PC window management, see DesktopWindow.h.

#include "platform/desktop/DesktopWindow.h"

#include <chrono>
#include <cmath>
#include <map>
#include <thread>

#include "cocos2d.h"
#include "MainMenu.h"
#include "qol/KeyBindings.h"
#include "qol/QoL.h"

USING_NS_CC;

namespace openwheels {
namespace desktop {
namespace {

GLViewImpl* g_view = nullptr;
int g_windowed[4] = {0, 0, 0, 0};

// Before the game set its design resolution (i.e. before Application::run), GLViewImpl ignores
// window size callbacks; read the size GLFW settled on and hand it over ourselves. Window managers
// (X11 especially) apply maximize / fullscreen asynchronously, so wait briefly for the change.
void adoptWindowSize(int previousWidth, int previousHeight)
{
    GLFWwindow* window = g_view->getWindow();
    int w = previousWidth, h = previousHeight;
    for (int i = 0; i < 50; ++i)
    {
        glfwPollEvents();
        glfwGetWindowSize(window, &w, &h);
        if (w != previousWidth || h != previousHeight) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (w > 0 && h > 0 && g_view->getResolutionPolicy() == ResolutionPolicy::UNKNOWN)
        g_view->setFrameSize((float)w, (float)h);
}

GLFWmonitor* monitorOf(GLFWwindow* window)
{
    int x, y, w, h;
    glfwGetWindowPos(window, &x, &y);
    glfwGetWindowSize(window, &w, &h);
    const int cx = x + w / 2, cy = y + h / 2;
    GLFWmonitor* best = glfwGetPrimaryMonitor();
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    for (int i = 0; i < count; ++i)
    {
        int mx, my;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        const GLFWvidmode* m = glfwGetVideoMode(monitors[i]);
        if (m && cx >= mx && cx < mx + m->width && cy >= my && cy < my + m->height) best = monitors[i];
    }
    return best;
}

// QOL page "fullscreen" (and F11): fullscreen at the monitor's current video mode (no mode switch,
// so no black bars and a fast toggle), restoring the windowed position and size afterwards.
void setFullscreen(bool on)
{
    GLFWwindow* window = g_view->getWindow();
    if (!window) return;
    const bool isFullscreen = glfwGetWindowMonitor(window) != nullptr;
    if (on == isFullscreen) return;
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    if (on)
    {
        glfwGetWindowPos(window, &g_windowed[0], &g_windowed[1]);
        g_windowed[2] = w;
        g_windowed[3] = h;
        GLFWmonitor* monitor = monitorOf(window);
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (!mode) return;
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else
    {
        if (g_windowed[2] <= 0 || g_windowed[3] <= 0)
        {
            g_windowed[2] = w * 3 / 4;
            g_windowed[3] = h * 3 / 4;
            g_windowed[0] = w / 8;
            g_windowed[1] = h / 8;
        }
        glfwSetWindowMonitor(window, nullptr, g_windowed[0], g_windowed[1], g_windowed[2], g_windowed[3], 0);
    }
    adoptWindowSize(w, h);
}

// ---------------------------------------------------------------------------------------------
// Runtime resize: keep the running scene centred and stretch its full-screen backgrounds.

struct Stretched
{
    float scaleX;
    float positionX;
};

Scene* g_layoutScene = nullptr;        // scene the bookkeeping below belongs to (not retained)
float g_builtWidth = 0.0f;             // design width that scene was laid out for
std::map<Node*, Stretched> g_stretched;  // its full-screen backgrounds (retained) and their originals

void forgetScene()
{
    for (auto& entry : g_stretched) entry.first->release();
    g_stretched.clear();
    g_layoutScene = nullptr;
}

bool isMainMenuScene(Scene* scene)
{
    for (Node* child : scene->getChildren())
        if (dynamic_cast<MainMenu*>(child)) return true;
    return false;
}

// Full-screen backgrounds: LayerColors and sprites that covered the whole design area the scene
// was built for, in a parent sitting at the scene origin without scaling (the original's
// MenuHelper::addBg sprite, BackgroundLayer's colour layer, Gameplay's blue layer, overlays).
void collectBackgrounds(Node* node, int depth, float builtWidth, float height)
{
    if (depth > 4) return;
    for (Node* child : node->getChildren())
    {
        if (g_stretched.count(child) == 0)
        {
            const Mat4& parentToWorld = node->getNodeToWorldTransform();
            const bool plainParent = std::fabs(parentToWorld.m[0] - 1.0f) < 1e-3f &&
                                     std::fabs(parentToWorld.m[5] - 1.0f) < 1e-3f &&
                                     std::fabs(parentToWorld.m[1]) < 1e-3f && std::fabs(parentToWorld.m[4]) < 1e-3f;
            const Rect box = child->getBoundingBox();
            const bool fullScreen = plainParent && child->getRotation() == 0.0f &&
                                    (dynamic_cast<LayerColor*>(child) || dynamic_cast<Sprite*>(child)) &&
                                    box.getMinX() <= 1.0f && box.getMaxX() >= builtWidth - 1.0f &&
                                    box.getMinY() <= 1.0f && box.getMaxY() >= height - 1.0f &&
                                    box.size.width <= builtWidth + 2.0f;
            if (fullScreen)
            {
                child->retain();
                g_stretched[child] = {child->getScaleX(), child->getPositionX()};
                continue;
            }
        }
        collectBackgrounds(child, depth + 1, builtWidth, height);
    }
}

void onWindowResized(const Size& before)
{
    Director* director = Director::getInstance();
    Scene* scene = director->getRunningScene();
    if (!scene) return;
    const Size now = director->getVisibleSize();
    if (scene != g_layoutScene)
    {
        forgetScene();
        g_layoutScene = scene;
        g_builtWidth = before.width;
    }
    if (std::fabs(now.width - g_builtWidth) < 1.0f && std::fabs(scene->getPositionX()) < 0.5f) return;

    if (isMainMenuScene(scene))
    {
        // The title screen has no state worth keeping: lay it out again for the new size.
        forgetScene();
        director->replaceScene(MainMenu::createScene(MenuModeMain, nullptr));
        return;
    }

    collectBackgrounds(scene, 0, g_builtWidth, now.height);
    const float dx = (now.width - g_builtWidth) * 0.5f;
    scene->setPositionX(dx);
    const float factor = now.width / g_builtWidth;
    for (auto& entry : g_stretched)
    {
        Node* node = entry.first;
        if (factor <= 1.0f)
        {
            node->setScaleX(entry.second.scaleX);
            node->setPositionX(entry.second.positionX);
            continue;
        }
        node->setScaleX(entry.second.scaleX * factor);
        // Keep the node's left edge at the window's left edge (scene x = -dx).
        node->setPositionX(entry.second.positionX);
        const float left = node->getBoundingBox().getMinX();
        node->setPositionX(entry.second.positionX - dx - left);
    }
}

}  // namespace

void installWindowManagement(GLViewImpl* glview, bool startMaximized, bool interactive)
{
    g_view = glview;
    qol::setFullscreenHandler([](bool on) { setFullscreen(on); });
    if (!interactive) return;

    GLFWwindow* window = glview->getWindow();
    // Settle the final size before any scene is laid out: saved fullscreen, else maximized.
    if (qol::fullscreen())
    {
        setFullscreen(true);
    }
    else if (startMaximized && window)
    {
        int w, h;
        glfwGetWindowSize(window, &w, &h);
        glfwMaximizeWindow(window);
        adoptWindowSize(w, h);
    }

    auto keys = EventListenerKeyboard::create();
    keys->onKeyPressed = [](EventKeyboard::KeyCode key, Event*) {
        // QOL (PC addition): F11 by default, remappable (src/qol/KeyBindings.h).
        if (qol::keyIs(key, qol::KeyAction::Fullscreen)) qol::setFullscreen(!qol::fullscreen());
    };
    Director* director = Director::getInstance();
    director->getEventDispatcher()->addEventListenerWithFixedPriority(keys, 2);

    // GLViewImpl re-applies FIXED_HEIGHT on every window size change (the design width follows the
    // new aspect); notice the change once per frame and fix up the running scene.
    static int s_resizeTarget = 0;
    static Size s_lastDesign;
    director->getScheduler()->schedule(
        [](float) {
            const Size now = Director::getInstance()->getVisibleSize();
            if (s_lastDesign.width > 0.0f && std::fabs(now.width - s_lastDesign.width) >= 1.0f)
                onWindowResized(s_lastDesign);
            s_lastDesign = now;
        },
        &s_resizeTarget, 0.0f, false, "ow_window_resize");

    static int s_applyTarget = 0;
    director->getScheduler()->schedule([](float) { qol::applyDisplaySettings(); }, &s_applyTarget, 0.0f, 0, 0.5f,
                                       false, "ow_apply_display");
}

}  // namespace desktop
}  // namespace openwheels
