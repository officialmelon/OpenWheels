// PAD (PC addition): controller focus navigation for every menu, see MenuFocus.h.
#include "input/MenuFocus.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "cocos2d.h"
#include "extensions/GUI/CCScrollView/CCScrollView.h"
#include "ui/UIScrollView.h"
#include "ui/UIWidget.h"

#include "HWWindow.h"
#include "LevelSelectBtn.h"
#include "PageControl.h"
#include "PauseLayer.h"
#include "Session.h"
#include "input/Gamepad.h"
#include "online/OnlineUi.h"
#include "qol/PadBindings.h"
#include "qol/QoLWidgets.h"

USING_NS_CC;

namespace openwheels {
namespace menufocus {
namespace {

const intptr_t kMenuFinger = 7000;  // injected touch id (keyboard fingers: 1000+, actions: 5000+)
const char* const kFocusName = "ow_focus";
const char* const kBackName = "ow_back";
const int kNodeBudget = 30000;      // nodes visited per scan (the paused level's world is large)
const float kRepeatDelay = 0.38f;   // held direction: first repeat
const float kRepeatEvery = 0.11f;   // then
const float kScanEvery = 0.2f;      // re-validate the focus this often while it shows

struct Candidate
{
    Node* node = nullptr;
    Rect rect;                       // world
    Node* modal = nullptr;           // innermost modal root it is in (null: none)
    Node* scroller = nullptr;        // ScrollView it is in (null: none)
    Rect view;                       // that scroller's view (world)
    bool clipped = false;            // outside the scroller's view (scrolled into it on focus)
};

struct Scan
{
    std::vector<Candidate> all;
    Node* topModal = nullptr;        // the last-drawn (topmost) modal root, null when none
    PauseLayer* pause = nullptr;
    bool pages = false;              // a PageControl on screen (level select chapters)
    std::vector<Candidate> active;   // candidates of the topmost modal (or all when none)
};

struct State
{
    Scene* scene = nullptr;
    RefPtr<Node> focus;
    RefPtr<Node> overlay;            // the Director's notification node
    DrawNode* glow = nullptr;
    DrawNode* pointerDot = nullptr;
    bool pointerMode = false;
    Vec2 pointer;                    // world
    bool selectDown = false;         // a menu finger is down
    Vec2 selectAt;                   // world point it went down at
    int heldDir = -1;                // 0 up, 1 down, 2 left, 3 right
    float heldFor = 0.0f;
    float nextRepeat = 0.0f;
    float scanTimer = 0.0f;
    float scrollTimer = 0.0f;
    float clock = 0.0f;
    bool wasGameplay = false;
};

State& st()
{
    static State s;
    return s;
}

// ---- geometry ------------------------------------------------------------------------------------

Rect toWorld(Node* node, const Rect& local)
{
    return RectApplyAffineTransform(local, node->getNodeToWorldAffineTransform());
}

// The node's content rect, or the union of its children's when it has no size (plain containers).
Rect localBounds(Node* node)
{
    const Size size = node->getContentSize();
    if (size.width > 1.0f && size.height > 1.0f) return Rect(Vec2::ZERO, size);
    Rect r;
    bool any = false;
    for (Node* child : node->getChildren())
    {
        if (!child->isVisible()) continue;
        const Rect b = child->getBoundingBox();
        if (b.size.width <= 0.0f || b.size.height <= 0.0f) continue;
        r = any ? r.unionWithRect(b) : b;
        any = true;
    }
    return r;
}

Rect screenRect()
{
    Director* d = Director::getInstance();
    return Rect(d->getVisibleOrigin(), d->getVisibleSize());
}

Rect intersect(const Rect& a, const Rect& b)
{
    const float x0 = std::max(a.getMinX(), b.getMinX()), y0 = std::max(a.getMinY(), b.getMinY());
    const float x1 = std::min(a.getMaxX(), b.getMaxX()), y1 = std::min(a.getMaxY(), b.getMaxY());
    return Rect(x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0));
}

bool shown(Node* node)
{
    for (Node* n = node; n; n = n->getParent())
    {
        if (!n->isVisible()) return false;
    }
    return node->isRunning();
}

// ---- injected input --------------------------------------------------------------------------------

void worldToScreen(const Vec2& world, float* x, float* y)
{
    auto director = Director::getInstance();
    auto glview = director->getOpenGLView();
    const Vec2 ui = director->convertToUI(world);
    const Rect vp = glview->getViewPortRect();
    *x = ui.x * glview->getScaleX() + vp.origin.x;
    *y = ui.y * glview->getScaleY() + vp.origin.y;
}

enum Phase { kBegin, kMove, kEnd };

void inject(Phase phase, const Vec2& world)
{
    GLView* glview = Director::getInstance()->getOpenGLView();
    if (!glview) return;
    float x, y;
    worldToScreen(world, &x, &y);
    intptr_t id = kMenuFinger;
    pad::setSyntheticInput(true);
    if (phase == kBegin) glview->handleTouchesBegin(1, &id, &x, &y);
    else if (phase == kMove) glview->handleTouchesMove(1, &id, &x, &y);
    else glview->handleTouchesEnd(1, &id, &x, &y);
    pad::setSyntheticInput(false);
}

void tap(const Vec2& world)
{
    inject(kBegin, world);
    inject(kEnd, world);
}

// A quick horizontal swipe across the middle of the screen (level select: next / previous chapter).
void swipe(int direction)
{
    const Rect screen = screenRect();
    const Vec2 from(screen.getMidX(), screen.getMidY());
    const Vec2 to(from.x - direction * screen.size.width * 0.3f, from.y);
    inject(kBegin, from);
    inject(kMove, from.lerp(to, 0.5f));
    inject(kMove, to);
    inject(kEnd, to);
}

void key(EventKeyboard::KeyCode code)
{
    pad::setSyntheticInput(true);
    EventDispatcher* dispatcher = Director::getInstance()->getEventDispatcher();
    EventKeyboard down(code, true);
    dispatcher->dispatchEvent(&down);
    EventKeyboard up(code, false);
    dispatcher->dispatchEvent(&up);
    pad::setSyntheticInput(false);
}

void mouseScroll(const Vec2& world, float amount)
{
    pad::setSyntheticInput(true);
    EventMouse e(EventMouse::MouseEventType::MOUSE_SCROLL);
    e.setCursorPosition(world.x, world.y);
    e.setScrollData(0.0f, amount);
    Director::getInstance()->getEventDispatcher()->dispatchEvent(&e);
    pad::setSyntheticInput(false);
}

void mouseMove(const Vec2& world)
{
    pad::setSyntheticInput(true);
    EventMouse e(EventMouse::MouseEventType::MOUSE_MOVE);
    e.setCursorPosition(world.x, world.y);
    Director::getInstance()->getEventDispatcher()->dispatchEvent(&e);
    pad::setSyntheticInput(false);
}

// ---- scrolling -------------------------------------------------------------------------------------

// Moves a scroll view's content by `world` (design units, world space). True when it moved.
bool scrollContent(Node* scroller, Vec2 world)
{
    const AffineTransform t = scroller->getNodeToWorldAffineTransform();
    const float sx = std::max(0.0001f, std::sqrt(t.a * t.a + t.b * t.b));
    const float sy = std::max(0.0001f, std::sqrt(t.c * t.c + t.d * t.d));
    const Vec2 local(world.x / sx, world.y / sy);
    if (auto sv = dynamic_cast<extension::ScrollView*>(scroller))
    {
        const Vec2 lo = sv->minContainerOffset(), hi = sv->maxContainerOffset();
        Vec2 offset = sv->getContentOffset() + local;
        offset.x = std::max(std::min(lo.x, hi.x), std::min(std::max(lo.x, hi.x), offset.x));
        offset.y = std::max(std::min(lo.y, hi.y), std::min(std::max(lo.y, hi.y), offset.y));
        if (offset.equals(sv->getContentOffset())) return false;
        sv->setContentOffset(offset, false);
        return true;
    }
    if (auto sv = dynamic_cast<ui::ScrollView*>(scroller))
    {
        const Size view = sv->getContentSize(), inner = sv->getInnerContainerSize();
        Vec2 pos = sv->getInnerContainerPosition() + local;
        pos.x = std::max(std::min(0.0f, view.width - inner.width), std::min(0.0f, pos.x));
        pos.y = std::max(std::min(0.0f, view.height - inner.height), std::min(0.0f, pos.y));
        if (pos.equals(sv->getInnerContainerPosition())) return false;
        sv->setInnerContainerPosition(pos);
        return true;
    }
    return false;
}

// Scrolls `c` fully into its scroller's view.
void scrollIntoView(const Candidate& c)
{
    if (!c.scroller) return;
    const float margin = 20.0f;
    Vec2 delta;
    if (c.rect.getMaxY() > c.view.getMaxY()) delta.y = -(c.rect.getMaxY() - c.view.getMaxY() + margin);
    else if (c.rect.getMinY() < c.view.getMinY()) delta.y = c.view.getMinY() - c.rect.getMinY() + margin;
    if (c.rect.getMaxX() > c.view.getMaxX()) delta.x = -(c.rect.getMaxX() - c.view.getMaxX() + margin);
    else if (c.rect.getMinX() < c.view.getMinX()) delta.x = c.view.getMinX() - c.rect.getMinX() + margin;
    if (delta != Vec2::ZERO) scrollContent(c.scroller, delta);
}

// ---- scanning ----------------------------------------------------------------------------------------

// True (and the node's world rect) for the things a finger can press.
bool focusable(Node* node, Rect* rect)
{
    if (auto item = dynamic_cast<MenuItem*>(node))
    {
        auto menu = dynamic_cast<Menu*>(item->getParent());
        if (!item->isEnabled() || !menu || !menu->isEnabled()) return false;
        *rect = toWorld(item, Rect(Vec2::ZERO, item->getContentSize()));
        return true;
    }
    if (auto level = dynamic_cast<LevelSelectBtn*>(node))
    {
        if (level->getChapter() < 0 || level->getLevel() < 0 || !level->getParent()) return false;
        *rect = toWorld(level->getParent(), level->getHitArea());
        return true;
    }
    if (auto button = dynamic_cast<online::ui::Button*>(node))
    {
        if (!button->isEnabled()) return false;
        *rect = toWorld(node, localBounds(node));
        return true;
    }
    if (dynamic_cast<online::ui::SearchField*>(node) || node->getName() == kFocusName)
    {
        *rect = toWorld(node, localBounds(node));
        return true;
    }
    if (auto widget = dynamic_cast<ui::Widget*>(node))
    {
        if (dynamic_cast<ui::Layout*>(widget) || !widget->isTouchEnabled() || !widget->isEnabled()) return false;
        *rect = toWorld(node, Rect(Vec2::ZERO, node->getContentSize()));
        return true;
    }
    return false;
}

bool isModalRoot(Node* node)
{
    return dynamic_cast<HWWindow*>(node) || node->getName() == online::ui::kModalNodeName;
}

void visit(Node* node, Rect clip, Node* modal, Node* scroller, Rect view, Scan& scan, int& budget)
{
    if (--budget < 0 || !node->isVisible()) return;
    if (dynamic_cast<Session*>(node)) return;  // the level's world: nothing to press in there
    if (isModalRoot(node))
    {
        modal = node;
        scan.topModal = node;  // visited in draw order: the last one is on top
    }
    if (auto pause = dynamic_cast<PauseLayer*>(node)) scan.pause = pause;
    if (dynamic_cast<PageControl*>(node)) scan.pages = true;

    Rect rect;
    if (focusable(node, &rect))
    {
        if (rect.size.width < 4.0f || rect.size.height < 4.0f) return;
        const Vec2 centre(rect.getMidX(), rect.getMidY());
        Candidate c;
        c.node = node;
        c.rect = rect;
        c.modal = modal;
        c.scroller = scroller;
        c.view = view;
        if (!clip.containsPoint(centre))
        {
            // Hidden by a scroll view (reachable by scrolling it) or off screen / clipped (not).
            if (!scroller || !screenRect().containsPoint(Vec2(view.getMidX(), view.getMidY()))) return;
            c.clipped = true;
        }
        scan.all.push_back(c);
        return;  // a button's children are its art
    }

    // Clipping containers narrow what is visible below them.
    if (auto sv = dynamic_cast<extension::ScrollView*>(node))
    {
        view = toWorld(node, Rect(Vec2::ZERO, sv->getViewSize()));
        clip = intersect(clip, view);
        scroller = node;
    }
    else if (auto sv = dynamic_cast<ui::ScrollView*>(node))
    {
        view = toWorld(node, Rect(Vec2::ZERO, sv->getContentSize()));
        clip = intersect(clip, view);
        scroller = node;
    }
    else if (auto layout = dynamic_cast<ui::Layout*>(node))
    {
        if (layout->isClippingEnabled()) clip = intersect(clip, toWorld(node, Rect(Vec2::ZERO, node->getContentSize())));
    }
    else if (auto rectClip = dynamic_cast<ClippingRectangleNode*>(node))
    {
        if (rectClip->isClippingEnabled()) clip = intersect(clip, toWorld(node, rectClip->getClippingRegion()));
    }

    // Children in draw order.
    std::vector<Node*> children(node->getChildren().begin(), node->getChildren().end());
    std::stable_sort(children.begin(), children.end(),
                     [](Node* a, Node* b) { return a->getLocalZOrder() < b->getLocalZOrder(); });
    for (Node* child : children) visit(child, clip, modal, scroller, view, scan, budget);
}

bool inside(Node* node, Node* root)
{
    for (Node* n = node; n; n = n->getParent())
    {
        if (n == root) return true;
    }
    return false;
}

Scan scanScene(Scene* scene)
{
    Scan scan;
    if (!scene || dynamic_cast<TransitionScene*>(scene)) return scan;
    int budget = kNodeBudget;
    visit(scene, screenRect(), nullptr, nullptr, Rect::ZERO, scan, budget);
    for (const Candidate& c : scan.all)
    {
        if (!scan.topModal || inside(c.node, scan.topModal)) scan.active.push_back(c);
    }
    return scan;
}

const Candidate* find(const Scan& scan, Node* node)
{
    for (const Candidate& c : scan.active)
    {
        if (c.node == node) return &c;
    }
    return nullptr;
}

// Where the highlight starts: the pause menu's resume button, a popup's last (confirm) button,
// otherwise the button nearest the middle of the screen.
const Candidate* defaultFocus(const Scan& scan)
{
    if (scan.active.empty()) return nullptr;
    if (scan.pause && !scan.topModal)
    {
        for (const Candidate& c : scan.active)
        {
            if (inside(c.node, scan.pause) && c.node->getTag() == 0) return &c;
        }
    }
    if (scan.topModal)
    {
        for (auto it = scan.active.rbegin(); it != scan.active.rend(); ++it)
        {
            if (!it->clipped) return &*it;
        }
    }
    const Rect screen = screenRect();
    const Vec2 middle(screen.getMidX(), screen.getMidY());
    const Candidate* best = nullptr;
    float bestDistance = 0.0f;
    for (const Candidate& c : scan.active)
    {
        if (c.clipped) continue;
        const float d = Vec2(c.rect.getMidX(), c.rect.getMidY()).distance(middle);
        if (!best || d < bestDistance)
        {
            best = &c;
            bestDistance = d;
        }
    }
    return best ? best : &scan.active.front();
}

// Spatial navigation: the nearest candidate in `dir` (0 up, 1 down, 2 left, 3 right), favouring
// ones lined up with the current one.
const Candidate* neighbour(const Scan& scan, const Rect& from, int dir)
{
    static const Vec2 kDirs[4] = {Vec2(0, 1), Vec2(0, -1), Vec2(-1, 0), Vec2(1, 0)};
    const Vec2 d = kDirs[dir];
    const bool vertical = dir < 2;
    const Vec2 c0(from.getMidX(), from.getMidY());
    const Candidate* best = nullptr;
    float bestScore = 0.0f;
    for (const Candidate& c : scan.active)
    {
        if (c.rect.equals(from)) continue;
        const Vec2 c1(c.rect.getMidX(), c.rect.getMidY());
        const float along = (c1 - c0).dot(d);
        if (along < 8.0f) continue;
        // Gap between the two rects across the direction (0 when they overlap).
        float gap;
        if (vertical) gap = std::max(0.0f, std::max(c.rect.getMinX() - from.getMaxX(), from.getMinX() - c.rect.getMaxX()));
        else gap = std::max(0.0f, std::max(c.rect.getMinY() - from.getMaxY(), from.getMinY() - c.rect.getMaxY()));
        const float offAxis = vertical ? std::fabs(c1.x - c0.x) : std::fabs(c1.y - c0.y);
        if (gap > along * 3.0f + 200.0f) continue;  // far off to the side
        const float score = along + gap * 2.5f + offAxis * 0.15f;
        if (!best || score < bestScore)
        {
            best = &c;
            bestScore = score;
        }
    }
    return best;
}

// ---- drawing -------------------------------------------------------------------------------------------

void ensureOverlay()
{
    State& s = st();
    Director* director = Director::getInstance();
    if (s.overlay && director->getNotificationNode() == s.overlay.get()) return;
    if (director->getNotificationNode() && !s.overlay) return;  // someone else's: leave it alone
    Node* root = Node::create();
    s.glow = DrawNode::create();
    root->addChild(s.glow);
    s.pointerDot = DrawNode::create();
    root->addChild(s.pointerDot);
    s.overlay = root;
    director->setNotificationNode(root);
}

void strokeRect(DrawNode* node, const Rect& r, float radius, const Color4F& colour)
{
    const Vec2 a(r.getMinX(), r.getMinY()), b(r.getMaxX(), r.getMinY());
    const Vec2 c(r.getMaxX(), r.getMaxY()), d(r.getMinX(), r.getMaxY());
    node->drawSegment(a, b, radius, colour);
    node->drawSegment(b, c, radius, colour);
    node->drawSegment(c, d, radius, colour);
    node->drawSegment(d, a, radius, colour);
}

// The highlight: a pulsing yellow outline with a soft glow around the focused button.
void drawGlow(const Rect& rect)
{
    State& s = st();
    s.glow->clear();
    const float pulse = 0.75f + 0.25f * std::sin(s.clock * 5.0f);
    const Rect r(rect.origin.x - 14.0f, rect.origin.y - 14.0f, rect.size.width + 28.0f, rect.size.height + 28.0f);
    const Color4F yellow(1.0f, 0.86f, 0.1f, 1.0f);
    s.glow->drawSolidRect(r.origin, Vec2(r.getMaxX(), r.getMaxY()), Color4F(1.0f, 0.95f, 0.6f, 0.10f * pulse));
    strokeRect(s.glow, r, 26.0f, Color4F(yellow.r, yellow.g, yellow.b, 0.12f * pulse));
    strokeRect(s.glow, r, 15.0f, Color4F(yellow.r, yellow.g, yellow.b, 0.30f * pulse));
    strokeRect(s.glow, r, 6.0f, Color4F(yellow.r, yellow.g, yellow.b, 0.95f * pulse + 0.05f));
}

void drawPointer(const Vec2& at, bool down)
{
    State& s = st();
    s.pointerDot->clear();
    s.pointerDot->drawDot(at, down ? 30.0f : 36.0f, Color4F(0.0f, 0.0f, 0.0f, 0.55f));
    s.pointerDot->drawDot(at, down ? 22.0f : 26.0f, Color4F(1.0f, 0.86f, 0.1f, 1.0f));
    s.pointerDot->drawDot(at, 8.0f, Color4F(1.0f, 1.0f, 1.0f, 1.0f));
}

// ---- actions ---------------------------------------------------------------------------------------------

void releaseSelect()
{
    State& s = st();
    if (!s.selectDown) return;
    s.selectDown = false;
    inject(kEnd, s.selectAt);
}

void setFocus(const Candidate* c)
{
    State& s = st();
    s.focus = c ? c->node : nullptr;
    if (c && c->clipped) scrollIntoView(*c);
}

// "menu: back": the pause menu's resume, a popup's cancel / close / only button, the screen's back
// button, or Escape (the screens and panels that close on it).
void back(const Scan& scan)
{
    if (scan.pause && !scan.topModal)
    {
        for (const Candidate& c : scan.active)
        {
            if (inside(c.node, scan.pause) && c.node->getTag() == 0)
            {
                tap(Vec2(c.rect.getMidX(), c.rect.getMidY()));
                return;
            }
        }
    }
    if (scan.topModal && dynamic_cast<HWWindow*>(scan.topModal))
    {
        const Candidate* pick = nullptr;
        for (const Candidate& c : scan.active)
        {
            if (!dynamic_cast<MenuItem*>(c.node)) continue;
            if (c.node->getTag() == 0) { pick = &c; break; }  // cancel
        }
        if (!pick && scan.active.size() == 1) pick = &scan.active.front();  // OK
        if (!pick)
        {
            for (const Candidate& c : scan.active)  // the close (x) button: tag -1, at the top
            {
                if (c.node->getTag() == -1 && (!pick || c.rect.getMidY() > pick->rect.getMidY())) pick = &c;
            }
        }
        if (pick) tap(Vec2(pick->rect.getMidX(), pick->rect.getMidY()));
        return;
    }
    if (scan.topModal)
    {
        if (online::ui::Dropdown::anyOpen()) online::ui::Dropdown::closeAll();
        else key(EventKeyboard::KeyCode::KEY_ESCAPE);
        return;
    }
    for (const Candidate& c : scan.active)
    {
        if (c.node->getName() == kBackName && !c.clipped)
        {
            tap(Vec2(c.rect.getMidX(), c.rect.getMidY()));
            return;
        }
    }
    key(EventKeyboard::KeyCode::KEY_ESCAPE);
}

// Held direction this frame (d-pad or left stick), -1 for none.
int heldDirection()
{
    struct Dir { int dpad, stick; };
    static const Dir kDirs[4] = {{pad::kDUp, pad::kLUp}, {pad::kDDown, pad::kLDown},
                                 {pad::kDLeft, pad::kLLeft}, {pad::kDRight, pad::kLRight}};
    int best = -1;
    float bestValue = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        const float v = std::max(pad::held(kDirs[i].dpad) ? 1.0f : 0.0f, pad::held(kDirs[i].stick) ? pad::value(kDirs[i].stick) : 0.0f);
        if (v > bestValue)
        {
            best = i;
            bestValue = v;
        }
    }
    return best;
}

// Direction presses with auto-repeat: the direction on the frame it is pushed, then repeated.
int directionStep(float dt)
{
    State& s = st();
    const int dir = heldDirection();
    if (dir != s.heldDir)
    {
        s.heldDir = dir;
        s.heldFor = 0.0f;
        s.nextRepeat = kRepeatDelay;
        return dir;
    }
    if (dir < 0) return -1;
    s.heldFor += dt;
    if (s.heldFor >= s.nextRepeat)
    {
        s.nextRepeat += kRepeatEvery;
        return dir;
    }
    return -1;
}

void updatePointer(float dt, const Scan& scan)
{
    State& s = st();
    const Rect screen = screenRect();
    float x, y;
    pad::leftStick(&x, &y);
    if (pad::held(pad::kDLeft)) x = -1.0f;
    if (pad::held(pad::kDRight)) x = 1.0f;
    if (pad::held(pad::kDUp)) y = 1.0f;
    if (pad::held(pad::kDDown)) y = -1.0f;
    const float len = std::sqrt(x * x + y * y);
    if (len > 0.15f)
    {
        // Gentle near the centre, fast at full tilt: a screen height in ~0.8 s.
        const float speed = screen.size.height * 1.25f * std::pow(std::min(1.0f, len), 1.8f) / len;
        s.pointer.x += x * speed * dt;
        s.pointer.y += y * speed * dt;
        s.pointer.x = std::max(screen.getMinX(), std::min(screen.getMaxX(), s.pointer.x));
        s.pointer.y = std::max(screen.getMinY(), std::min(screen.getMaxY(), s.pointer.y));
        if (s.selectDown) inject(kMove, s.pointer);
        else mouseMove(s.pointer);
    }
    if (qol::padActionPressed(qol::PadAction::MenuSelect))
    {
        releaseSelect();
        s.selectDown = true;
        s.selectAt = s.pointer;
        inject(kBegin, s.pointer);
    }
    else if (s.selectDown && !qol::padActionHeld(qol::PadAction::MenuSelect))
    {
        s.selectDown = false;
        inject(kEnd, s.pointer);
    }
    if (qol::padActionPressed(qol::PadAction::MenuBack)) back(scan);
    s.glow->clear();
    drawPointer(s.pointer, s.selectDown);
}

void reset()
{
    State& s = st();
    releaseSelect();
    s.focus = nullptr;
    s.heldDir = -1;
    if (s.glow) s.glow->clear();
    if (s.pointerDot) s.pointerDot->clear();
}

}  // namespace

void update(float dt, bool gameplayActive)
{
    State& s = st();
    s.clock += dt;
    ensureOverlay();
    if (!s.overlay) return;

    Scene* scene = Director::getInstance()->getRunningScene();
    if (scene != s.scene)
    {
        reset();
        s.scene = scene;
        s.scanTimer = 0.0f;
    }
    if (pad::capturing())
    {
        // The controller page waits for an input: keep the highlight where it is, hidden.
        releaseSelect();
        if (s.glow) s.glow->clear();
        return;
    }
    if (gameplayActive || !pad::anyConnected())
    {
        if (!s.wasGameplay || s.focus || s.selectDown) reset();
        s.wasGameplay = true;
        return;
    }
    s.wasGameplay = false;

    // Focus mode <-> free pointer (R3).
    if (pad::pressed(pad::kRS))
    {
        releaseSelect();
        s.pointerMode = !s.pointerMode;
        const Rect screen = screenRect();
        Rect r;
        if (s.focus && focusable(s.focus.get(), &r)) s.pointer = Vec2(r.getMidX(), r.getMidY());
        else s.pointer = Vec2(screen.getMidX(), screen.getMidY());
        if (!s.pointerMode) s.pointerDot->clear();
    }

    const bool padInUse = pad::lastInputWasPad();
    const int step = directionStep(dt);
    const bool select = qol::padActionPressed(qol::PadAction::MenuSelect);
    const bool backPressed = qol::padActionPressed(qol::PadAction::MenuBack);
    const bool startPressed = pad::pressed(pad::kStart);  // resumes from the pause menu
    const bool page = pad::pressed(pad::kLB) || pad::pressed(pad::kRB);
    float rx, ry;
    pad::rightStick(&rx, &ry);
    const bool scrolling = std::fabs(ry) > 0.3f || std::fabs(rx) > 0.3f;

    s.scanTimer -= dt;
    const bool needScan = step >= 0 || select || backPressed || startPressed || page || scrolling || s.pointerMode ||
                          (padInUse && s.scanTimer <= 0.0f);
    if (!needScan)
    {
        if (!padInUse)
        {
            s.glow->clear();
            if (!s.pointerMode) s.pointerDot->clear();
        }
        else if (s.focus && !s.pointerMode && shown(s.focus.get()))
        {
            Rect rect;
            if (focusable(s.focus.get(), &rect)) drawGlow(rect);
        }
        // A finger held on a button that went away (scene changed under it) is lifted.
        if (s.selectDown && !qol::padActionHeld(qol::PadAction::MenuSelect)) releaseSelect();
        return;
    }
    s.scanTimer = kScanEvery;
    const Scan scan = scanScene(scene);

    if (s.pointerMode)
    {
        updatePointer(dt, scan);
        if (scrolling)
        {
            s.scrollTimer -= dt;
            if (s.scrollTimer <= 0.0f)
            {
                s.scrollTimer = 0.09f;
                mouseScroll(s.pointer, ry > 0.3f ? -1.0f : (ry < -0.3f ? 1.0f : 0.0f));
            }
        }
        return;
    }
    s.pointerDot->clear();

    const Candidate* focus = s.focus ? find(scan, s.focus.get()) : nullptr;
    if (!focus && s.focus) s.focus = nullptr;  // gone, hidden, or under a new popup

    // A finger let go of: lift it where it went down.
    if (s.selectDown && !qol::padActionHeld(qol::PadAction::MenuSelect)) releaseSelect();

    // Nothing highlighted yet: the first direction / select press only shows where the highlight
    // starts.
    bool consumed = false;
    if (!focus && (padInUse || step >= 0 || select))
    {
        const Candidate* start = defaultFocus(scan);
        setFocus(start);
        focus = start;
        consumed = focus && (step >= 0 || select);
    }

    if (step >= 0 && focus && !consumed)
    {
        auto slider = dynamic_cast<QoLSliderItem*>(focus->node);
        if (slider && step >= 2)
        {
            slider->nudge(step == 2 ? -1 : 1);
        }
        else if (const Candidate* next = neighbour(scan, focus->rect, step))
        {
            setFocus(next);
            focus = next;
        }
        else if (step >= 2 && scan.pages && !scan.topModal)
        {
            swipe(step == 3 ? 1 : -1);  // past the edge: next / previous chapter
        }
        else if (step < 2)
        {
            // Past the top / bottom of a list: scroll it.
            if (focus->scroller) scrollContent(focus->scroller, Vec2(0.0f, step == 0 ? -focus->rect.size.height : focus->rect.size.height));
            else if (focus->node->getName() == kFocusName) mouseScroll(Vec2(focus->rect.getMidX(), focus->rect.getMidY()), step == 0 ? -1.0f : 1.0f);
        }
    }

    if (page && scan.pages && !scan.topModal) swipe(pad::pressed(pad::kRB) ? 1 : -1);

    if (scrolling)
    {
        s.scrollTimer -= dt;
        if (s.scrollTimer <= 0.0f)
        {
            s.scrollTimer = 0.09f;
            const Rect screen = screenRect();
            const Vec2 at = focus ? Vec2(focus->rect.getMidX(), focus->rect.getMidY()) : Vec2(screen.getMidX(), screen.getMidY());
            if (focus && focus->scroller)
            {
                scrollContent(focus->scroller, Vec2(-rx, -ry) * 90.0f);
            }
            else if (std::fabs(ry) > 0.3f)
            {
                mouseScroll(at, ry > 0.0f ? -1.0f : 1.0f);
            }
        }
    }
    else
    {
        s.scrollTimer = 0.0f;
    }

    if (select && focus && !consumed && !dynamic_cast<QoLSliderItem*>(focus->node))
    {
        releaseSelect();
        if (focus->clipped) scrollIntoView(*focus);
        const Rect visible = focus->clipped ? focus->rect : intersect(focus->rect, focus->scroller ? focus->view : screenRect());
        s.selectDown = true;
        s.selectAt = Vec2(visible.getMidX(), visible.getMidY());
        inject(kBegin, s.selectAt);
    }
    if (backPressed && !s.selectDown) back(scan);
    else if (startPressed && scan.pause && !scan.topModal && !s.selectDown) back(scan);

    // The highlight (re-read: a press may have changed the scene or closed a popup).
    if (padInUse && s.focus && shown(s.focus.get()))
    {
        Rect rect;
        if (focusable(s.focus.get(), &rect)) drawGlow(rect);
        else s.glow->clear();
    }
    else
    {
        s.glow->clear();
    }
}

}  // namespace menufocus
}  // namespace openwheels
