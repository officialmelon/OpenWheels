// NET (PC addition): see NetUi.h.
#include "net/NetUi.h"

#include "HWWindow.h"
#include "UIKitCompat.h"
#include "online/OnlineUi.h"

USING_NS_CC;

namespace net {
namespace ui {

Label* label(const std::string& text, const std::string& font, float size, const Color3B& color, const Vec2& anchor) {
    Label* l = Label::createWithTTF(text, font, size);
    l->setColor(color);
    l->setAnchorPoint(anchor);
    return l;
}

bool Modal::initModal(const Size& panelSize, const std::string& title) {
    if (!Node::init()) return false;
    online::ui::loadAtlases();
    setName(online::ui::kModalNodeName);
    const Size vs = Director::getInstance()->getVisibleSize();
    const Vec2 origin = Director::getInstance()->getVisibleOrigin();

    _dim = LayerColor::create(Color4B(0, 0, 0, 190));
    _dim->setContentSize(vs);
    _dim->setPosition(origin);
    addChild(_dim, 0);

    _panelSize = panelSize;
    _panel = Node::create();
    _panel->setContentSize(panelSize);
    _panel->setAnchorPoint(Vec2(0.5f, 0.5f));
    _panel->setPosition(origin.x + vs.width * 0.5f, origin.y + vs.height * 0.5f);
    _panel->setCascadeOpacityEnabled(true);
    addChild(_panel, 1);
    Sprite* frame = online::ui::windowPanel(panelSize);
    frame->setAnchorPoint(Vec2::ZERO);
    _panel->addChild(frame, -1);
    _contentTop = panelSize.height - online::ui::windowPanelStrip();

    // HWWindow header: Clarendon 120, white, below the strip.
    _title = label(title, online::ui::kFontHeading, 112.0f, Color3B::WHITE, Vec2(0.5f, 0.5f));
    _title->enableShadow(Color4B(0, 0, 0, 70), Size(0.0f, -6.0f));
    _title->setPosition(panelSize.width * 0.5f, _contentTop - 100.0f);
    _panel->addChild(_title);

    // HWWindow's close button on the top-right corner.
    auto* close = MenuItemImage::create();
    close->setNormalImage(Sprite::createWithSpriteFrameName("window_btn_close.png"));
    Sprite* down = Sprite::createWithSpriteFrameName("window_btn_close.png");
    down->setColor(Color3B(190, 190, 190));
    close->setSelectedImage(down);
    close->setCallback([this](Ref*) { dismiss(); });
    close->setPosition(panelSize.width - 30.0f, panelSize.height - 34.0f);
    Menu* menu = Menu::create(close, nullptr);
    menu->setPosition(Vec2::ZERO);
    _panel->addChild(menu, 10);

    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (_closing) return true;
        onTouch(t->getLocation());
        return true;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);

    auto keys = EventListenerKeyboard::create();
    keys->onKeyPressed = [this](EventKeyboard::KeyCode key, Event* e) {
        if (_closing) return;
        // Only the topmost modal answers.
        Scene* scene = Director::getInstance()->getRunningScene();
        if (scene) {
            const auto& children = scene->getChildren();
            for (auto it = children.rbegin(); it != children.rend(); ++it) {
                if ((*it)->getName() == online::ui::kModalNodeName || dynamic_cast<HWWindow*>(*it)) {
                    if (*it != this) return;
                    break;
                }
            }
        }
        if (key == EventKeyboard::KeyCode::KEY_ESCAPE || key == EventKeyboard::KeyCode::KEY_BACK) {
            e->stopPropagation();
            dismiss();
            return;
        }
        if (onKey(key)) e->stopPropagation();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keys, this);

    auto mouse = EventListenerMouse::create();
    mouse->onMouseScroll = [this](EventMouse* e) {
        onScroll(Vec2(e->getCursorX(), e->getCursorY()), e->getScrollY());
        e->stopPropagation();
    };
    // Hover stops here, so the screen below does not light up its buttons through the dim.
    mouse->onMouseMove = [this](EventMouse* e) {
        onHover(Vec2(e->getCursorX(), e->getCursorY()));
        e->stopPropagation();
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, this);
    return true;
}

void Modal::present() {
    Scene* scene = Director::getInstance()->getRunningScene();
    if (!scene) return;
    scene->addChild(this, uikit::kWindowZOrder + 1);
    _dim->setOpacity(0);
    _dim->runAction(FadeTo::create(0.2f, 190));
    _panel->setScale(1.15f);
    _panel->setOpacity(0);
    _panel->runAction(Spawn::create(EaseBackOut::create(ScaleTo::create(0.25f, 1.0f)), FadeIn::create(0.18f), nullptr));
}

void Modal::dismiss() {
    if (_closing) return;
    _closing = true;
    RefPtr<Modal> keep(this);
    onClosed();
    _eventDispatcher->removeEventListenersForTarget(this);
    _dim->runAction(FadeTo::create(0.15f, 0));
    _panel->runAction(Sequence::create(Spawn::create(ScaleTo::create(0.15f, 0.92f), FadeOut::create(0.15f), nullptr),
                                       CallFunc::create([this]() { removeFromParent(); }), nullptr));
}

Node* deviceIcon(const std::string& platform, float s, const Color4F& c) {
    DrawNode* d = DrawNode::create();
    const float t = s * 0.045f;   // line half-thickness
    auto box = [&](float x0, float y0, float x1, float y1) {
        d->drawSegment(Vec2(x0, y0), Vec2(x1, y0), t, c);
        d->drawSegment(Vec2(x1, y0), Vec2(x1, y1), t, c);
        d->drawSegment(Vec2(x1, y1), Vec2(x0, y1), t, c);
        d->drawSegment(Vec2(x0, y1), Vec2(x0, y0), t, c);
    };
    if (platform == "phone") {
        box(-0.24f * s, -0.46f * s, 0.24f * s, 0.46f * s);
        d->drawSegment(Vec2(-0.07f * s, 0.36f * s), Vec2(0.07f * s, 0.36f * s), t * 0.8f, c);
        d->drawDot(Vec2(0.0f, -0.34f * s), t * 1.5f, c);
    } else {
        box(-0.46f * s, -0.12f * s, 0.46f * s, 0.42f * s);
        d->drawSolidRect(Vec2(-0.40f * s, -0.06f * s), Vec2(0.40f * s, 0.36f * s),
                         Color4F(c.r, c.g, c.b, c.a * 0.22f));
        d->drawSegment(Vec2(0.0f, -0.12f * s), Vec2(0.0f, -0.32f * s), t, c);
        d->drawSegment(Vec2(-0.2f * s, -0.34f * s), Vec2(0.2f * s, -0.34f * s), t, c);
    }
    d->setContentSize(Size::ZERO);
    return d;
}

void greyButton(HWWindow* window, int buttonTag) {
    if (!window) return;
    GLProgramState* grey = GLProgramState::getOrCreateWithGLProgramName(GLProgram::SHADER_NAME_POSITION_GRAYSCALE);
    std::function<void(Node*)> visit = [&](Node* n) {
        for (Node* child : n->getChildren()) {
            if (auto* item = dynamic_cast<MenuItemSprite*>(child)) {
                if (item->getTag() == buttonTag) {
                    if (auto* sprite = dynamic_cast<Sprite*>(item->getNormalImage())) sprite->setGLProgramState(grey);
                    if (auto* sprite = dynamic_cast<Sprite*>(item->getSelectedImage())) sprite->setGLProgramState(grey);
                }
            }
            visit(child);
        }
    };
    visit(window);
}

}  // namespace ui
}  // namespace net
