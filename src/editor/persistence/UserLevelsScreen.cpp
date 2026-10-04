// UI (PC addition): restyled - see UserLevelsScreen.h.
#include "UserLevelsScreen.h"

#include <algorithm>
#include <cmath>
#include <ctime>

#include "EditorLayer.h"
#include "Globals.h"
#include "HWWindow.h"
#include "LevelMO.h"
#include "LevelSession.h"
#include "LevelStore.h"
#include "LevelUIHelpers.h"
#include "MainMenu.h"
#include "MenuHelper.h"
#include "ShareAction.h"
#include "UIKitCompat.h"
#include "net/NearbyPanels.h"
#include "net/NetUi.h"
#include "online/OnlineUi.h"
#include "platform/common/Localization.h"

USING_NS_CC;

namespace oui = online::ui;

namespace {

const float G = 70.0f;                      // MainMenu's 70-unit grid
const Color3B kRowSelectedSub(225, 236, 255);
const int kTagDelete = 7201;
const int kChapters[2] = {LevelStoreChapterUser, LevelStoreChapterImported};

// Where the player left the screen (tab, selected level, scroll), so returning from a level or
// the editor comes back to the same place.
struct ScreenState {
    int tab = 0;
    int selectedId[2] = {-1, -1};
    float offset[2] = {0.0f, 0.0f};
};
ScreenState& screenState()
{
    static ScreenState s;
    return s;
}

Label* makeLabel(const std::string& text, const std::string& font, float size, const Color3B& color,
                 const Vec2& anchor = Vec2(0.0f, 0.5f))
{
    return net::ui::label(text, font, size, color, anchor);
}

// The online browser's rounded pill (portrait tags).
Node* makePill(const std::string& text, float fontSize, const Color3B& bg, GLubyte opacity, Label** labelOut)
{
    Node* n = Node::create();
    n->setCascadeOpacityEnabled(true);
    Label* l = makeLabel(text, oui::kFontBodyBold, fontSize, Color3B::WHITE, Vec2(0.5f, 0.5f));
    const Size ls = l->getContentSize();
    const Size size(ls.width + fontSize * 1.2f, fontSize * 1.6f);
    auto* b = oui::roundedRect(size, size.height * 0.5f, bg, opacity);
    b->setAnchorPoint(Vec2::ZERO);
    n->addChild(b, 0, 1);
    l->setPosition(size.width * 0.5f, size.height * 0.5f);
    n->addChild(l, 1, 2);
    n->setContentSize(size);
    n->setAnchorPoint(Vec2(0.5f, 0.5f));
    if (labelOut) *labelOut = l;
    return n;
}

void setPillText(Node* pill, Label* label, const std::string& text)
{
    label->setString(text);
    const float fs = label->getTTFConfig().fontSize;
    const Size size(label->getContentSize().width + fs * 1.2f, fs * 1.6f);
    if (auto* b = dynamic_cast<cocos2d::ui::Scale9Sprite*>(pill->getChildByTag(1))) b->setContentSize(size);
    label->setPosition(size.width * 0.5f, size.height * 0.5f);
    pill->setContentSize(size);
}

std::string formatTime(std::time_t t)
{
    if (t <= 0) return "";
    std::tm tm = *std::localtime(&t);
    char ymd[16];
    std::strftime(ymd, sizeof(ymd), "%Y-%m-%d", &tm);
    return oui::formatDate(ymd);
}

std::string levelName(LevelMO* l)
{
    std::string n = levelui::trimmed(l->name());
    return n.empty() ? "Untitled" : n;
}

// Character shown for a level: the forced one, else "any" (the player picks).
int shownCharacter(LevelMO* l)
{
    return l->force_character() ? l->playable_character() : 0;
}

}  // namespace

// ---- scene / build ---------------------------------------------------------------------------

Scene* UserLevelsScreen::createScene()
{
    Scene* scene = Scene::create();
    scene->addChild(UserLevelsScreen::create());
    return scene;
}

UserLevelsScreen::~UserLevelsScreen()
{
    uikit::NotificationCenter::removeObserver(this);
}

bool UserLevelsScreen::init()
{
    if (!Layer::init()) return false;
    _vs = Director::getInstance()->getVisibleSize();
    _origin = Director::getInstance()->getVisibleOrigin();
    _top = _origin.y + _vs.height;
    oui::loadAtlases();
    MenuHelper::addBg(this, -10);
    addChild(LayerColor::create(Color4B(10, 8, 22, 120)), -9);

    buildHeader();
    buildList();
    buildDetail();
    buildInput();
    reload();
    // Nothing of your own yet but levels from friends: open on Received.
    static bool s_firstOpen = true;
    if (s_firstOpen && _counts[0] == 0 && _counts[1] > 0) setTab(1);
    s_firstOpen = false;
    setListOffset(screenState().offset[screenState().tab]);
    scheduleUpdate();
    return true;
}

void UserLevelsScreen::onEnter()
{
    Layer::onEnter();
    // Levels may have changed in the editor (pushed over this scene) or arrived from a friend.
    uikit::NotificationCenter::addObserver(this, LevelStore::kLevelsChangedNotification, nullptr,
                                           [this](void*, void*) { reload(); });
    reload();
}

void UserLevelsScreen::onExit()
{
    uikit::NotificationCenter::removeObserver(this);
    Layer::onExit();
}

void UserLevelsScreen::buildHeader()
{
    const float h = 210.0f;
    const float cy = _top - G - h * 0.5f;

    auto* back = MenuItemImage::create();
    back->setNormalImage(Sprite::createWithSpriteFrameName("menu_main_back_light.png"));
    back->setSelectedImage(Sprite::createWithSpriteFrameName("menu_main_back_dark.png"));
    back->setCallback([this](Ref*) { goBack(); });
    back->setScale(h / back->getContentSize().height);
    back->setPosition(_origin.x + G + h * 0.5f, cy);
    Menu* menu = Menu::create(back, nullptr);
    menu->setPosition(Vec2::ZERO);
    addChild(menu, 5);

    Label* title = makeLabel("Your Levels", oui::kFontHeading, 124.0f, Color3B::WHITE);
    title->enableShadow(Color4B(0, 0, 0, 110), Size(0.0f, -8.0f));
    title->setPosition(_origin.x + G + h + 56.0f, cy + 6.0f);
    addChild(title, 5);
    float x = title->getPositionX() + title->getContentSize().width + 90.0f;

    // Tabs: the active one in the alert's blue button art, the other a translucent ghost.
    const Size tabSize(480.0f, 136.0f);
    for (int i = 0; i < 2; ++i)
    {
        _tabs[i] = oui::Button::create("", tabSize, oui::Button::ghost(), 54.0f);
        _tabs[i]->setPosition(x + tabSize.width * 0.5f, cy);
        _tabs[i]->setCallback([this, i]() { setTab(i); });
        addChild(_tabs[i], 5);
        x += tabSize.width + 30.0f;
    }

    // NEW LEVEL / RECEIVE on the right, in the main menu's chunky button art.
    const Size btn(500.0f, 170.0f);
    float right = _origin.x + _vs.width - G;
    auto* newBtn = oui::Button::create("New Level", btn, oui::Button::chunky("blue"), 66.0f);
    newBtn->setPosition(right - btn.width * 0.5f, cy);
    newBtn->setCallback([this]() { newLevel(); });
    addChild(newBtn, 5);
    right -= btn.width + 40.0f;
    auto* receiveBtn = oui::Button::create("Receive", btn, oui::Button::chunky("pink"), 66.0f);
    receiveBtn->setPosition(right - btn.width * 0.5f, cy);
    receiveBtn->setCallback([this]() { receive(); });
    addChild(receiveBtn, 5);
}

void UserLevelsScreen::buildList()
{
    const float inner = _vs.width - 3.0f * G;
    const float listW = std::floor(inner * 0.57f);
    const float x0 = _origin.x + G;
    const float top = _top - 340.0f;
    const float bottom = _origin.y + G;
    const float scrollW = 34.0f;
    _rowsRect = Rect(x0, bottom, listW - scrollW, top - bottom);

    auto* clip = ClippingRectangleNode::create(Rect(x0 - 10.0f, bottom, listW + 20.0f, top - bottom));
    addChild(clip, 2);
    _rowsNode = Node::create();
    clip->addChild(_rowsNode);

    const int poolSize = static_cast<int>(std::ceil(_rowsRect.size.height / _rowH)) + 2;
    const float rw = _rowsRect.size.width;
    const float rh = _rowH - 18.0f;
    const float midY = 18.0f + rh * 0.5f;
    for (int i = 0; i < poolSize; ++i)
    {
        Row r;
        r.node = Node::create();
        r.node->setContentSize(Size(rw, _rowH));
        _rowsNode->addChild(r.node);
        r.bg = oui::roundedRect(Size(rw, rh), 26.0f, Color3B(0, 0, 0), 100);
        r.bg->setAnchorPoint(Vec2::ZERO);
        r.bg->setPosition(0.0f, 18.0f);
        r.node->addChild(r.bg);
        r.selectedBg = oui::frameSprite("menu_main_btn_blue_normal.png", Size(rw, rh), Rect(0.25f, 0.25f, 0.5f, 0.5f));
        r.selectedBg->setAnchorPoint(Vec2::ZERO);
        r.selectedBg->setPosition(0.0f, 18.0f);
        r.node->addChild(r.selectedBg);
        r.portrait = Sprite::create();
        r.portrait->setPosition(120.0f, midY);
        r.node->addChild(r.portrait);
        r.tagNode = makePill("ANY", 28.0f, Color3B(40, 38, 62), 235, &r.tag);
        r.tagNode->setPosition(120.0f, 18.0f + 38.0f);
        r.node->addChild(r.tagNode);
        r.name = makeLabel("", oui::kFontHeading, 64.0f, Color3B::WHITE);
        r.name->setPosition(244.0f, midY + 30.0f);
        r.node->addChild(r.name);
        r.sub = makeLabel("", oui::kFontBody, 44.0f, oui::kTextDim);
        r.sub->setPosition(246.0f, midY - 48.0f);
        r.node->addChild(r.sub);
        r.node->setVisible(false);
        _rows.push_back(r);
    }

    _scrollThumb = oui::roundedRect(Size(14.0f, 100.0f), 7.0f, Color3B::WHITE, 110);
    _scrollThumb->setAnchorPoint(Vec2(0.0f, 1.0f));
    addChild(_scrollThumb, 3);

    _listMessage = Node::create();
    _listMessage->setPosition(_rowsRect.getMidX(), _rowsRect.getMidY() + 60.0f);
    addChild(_listMessage, 4);
}

void UserLevelsScreen::buildDetail()
{
    const float inner = _vs.width - 3.0f * G;
    const float listW = std::floor(inner * 0.57f);
    const float x0 = _origin.x + 2.0f * G + listW;
    const float w = _origin.x + _vs.width - G - x0;
    const float top = _top - 340.0f;
    const float bottom = _origin.y + G;
    _detailRect = Rect(x0, bottom, w, top - bottom);

    Sprite* panel = oui::windowPanel(_detailRect.size);
    panel->setAnchorPoint(Vec2::ZERO);
    panel->setPosition(_detailRect.origin);
    addChild(panel, 1);
    const float strip = oui::windowPanelStrip();

    _detailEmpty = Node::create();
    addChild(_detailEmpty, 2);
    if (Sprite* portrait = Sprite::create(oui::characterPortrait(0)))
    {
        portrait->setScale(360.0f / portrait->getContentSize().height);
        portrait->setOpacity(70);
        portrait->setPosition(_detailRect.getMidX(), _detailRect.getMidY() + 120.0f);
        _detailEmpty->addChild(portrait);
    }
    Label* hint = makeLabel("Pick a level", oui::kFontHeading, 64.0f, oui::kInkDim, Vec2(0.5f, 0.5f));
    hint->setPosition(_detailRect.getMidX(), _detailRect.getMidY() - 160.0f);
    _detailEmpty->addChild(hint, 0, 1);

    _detail = Node::create();
    addChild(_detail, 2);
    const float P = 70.0f;
    const float L = x0 + P;
    const float R = x0 + w - P;
    float y = top - strip - 46.0f;

    const float nameH = 210.0f;
    _detailName = Label::createWithTTF("", oui::kFontHeading, 92.0f, Size(R - L, 0.0f), TextHAlignment::LEFT,
                                       TextVAlignment::BOTTOM);
    _detailName->setColor(oui::kInk);
    _detailName->setAnchorPoint(Vec2(0.0f, 0.0f));
    _detailName->setPosition(L, y - nameH);
    _detail->addChild(_detailName);
    _detailNameMaxH = nameH;
    y -= nameH + 24.0f;

    // Portrait card; character / dates beside it.
    const Size avatar(250.0f, 280.0f);
    auto* card = oui::roundedRect(avatar, 30.0f, Color3B::WHITE, 255);
    card->setAnchorPoint(Vec2(0.0f, 1.0f));
    card->setPosition(L, y);
    _detail->addChild(card);
    _detailPortrait = Sprite::create();
    _detailPortrait->setPosition(L + avatar.width * 0.5f, y - avatar.height * 0.5f + 4.0f);
    _detail->addChild(_detailPortrait);
    _detailTag = makePill("ANY", 30.0f, Color3B(62, 62, 72), 235, &_detailTagLabel);
    _detailTag->setPosition(L + avatar.width * 0.5f, y - avatar.height + 38.0f);
    _detail->addChild(_detailTag);

    const float tx = L + avatar.width + 50.0f;
    Label* c1 = makeLabel("CHARACTER", oui::kFontBodyBold, 32.0f, oui::kInkDim);
    c1->setPosition(tx, y - 26.0f);
    _detail->addChild(c1);
    _detailChar = makeLabel("", oui::kFontBodyBold, 50.0f, oui::kInk);
    _detailChar->setPosition(tx, y - 76.0f);
    _detail->addChild(_detailChar);
    const float colW = (R - tx) * 0.5f;
    const char* captions[] = {"CREATED", "EDITED"};
    Label** values[] = {&_detailCreated, &_detailEdited};
    for (int i = 0; i < 2; ++i)
    {
        Label* c = makeLabel(captions[i], oui::kFontBodyBold, 32.0f, oui::kInkDim);
        c->setPosition(tx + colW * i, y - 160.0f);
        _detail->addChild(c);
        *values[i] = makeLabel("", oui::kFontBodyBold, 44.0f, oui::kInk);
        (*values[i])->setPosition(tx + colW * i, y - 206.0f);
        _detail->addChild(*values[i]);
    }
    _detailSource = makeLabel("", oui::kFontBody, 38.0f, oui::kInkDim);
    _detailSource->setPosition(tx, y - 260.0f);
    _detail->addChild(_detailSource);
    y -= avatar.height + 50.0f;

    // Bottom: PLAY, the secondary actions above it, the description between.
    const float playH = 250.0f;
    _playBtn = oui::Button::create("PLAY", Size(R - L, playH), oui::Button::playButton(), 116.0f);
    if (SpriteFrame* f = SpriteFrameCache::getInstance()->getSpriteFrameByName("menu_main_icon_play.png"))
    {
        Sprite* icon = Sprite::createWithTexture(f->getTexture(), f->getRect(), f->isRotated());
        icon->setScale(0.62f);
        _playBtn->setIcon(icon);
    }
    _playBtn->setPosition(L + (R - L) * 0.5f, bottom + P + playH * 0.5f);
    _playBtn->setCallback([this]() { playSelected(); });
    _detail->addChild(_playBtn);

    const float actionH = 128.0f;
    const float actionY = bottom + P + playH + 40.0f + actionH * 0.5f;
    struct Action
    {
        const char* text;
        const char* colour;
        float weight;
        void (UserLevelsScreen::*fn)();
    };
    const Action actions[] = {{"Edit", "blue", 1.0f, &UserLevelsScreen::editSelected},
                              {"Send to Nearby", "blue", 1.75f, &UserLevelsScreen::sendSelected},
                              {"Share", "blue", 1.0f, &UserLevelsScreen::shareSelected},
                              {"Delete", "pink", 1.0f, &UserLevelsScreen::deleteSelected}};
    const float gap = 26.0f;
    float weights = 0.0f;
    for (const Action& a : actions) weights += a.weight;
    const float unit = (R - L - gap * 3.0f) / weights;
    float ax = L;
    for (const Action& a : actions)
    {
        const float bw = unit * a.weight;
        auto* b = oui::Button::create(a.text, Size(bw, actionH), oui::Button::window(a.colour), 50.0f);
        b->setPosition(ax + bw * 0.5f, actionY);
        auto fn = a.fn;
        b->setCallback([this, fn]() { (this->*fn)(); });
        _detail->addChild(b);
        ax += bw + gap;
    }

    Label* descCaption = makeLabel("DESCRIPTION", oui::kFontBodyBold, 34.0f, oui::kInkDim);
    descCaption->setPosition(L, y - 18.0f);
    _detail->addChild(descCaption);
    const float boxTop = y - 56.0f;
    const float boxBottom = actionY + actionH * 0.5f + 40.0f;
    auto* box = oui::roundedRect(Size(R - L, std::max(60.0f, boxTop - boxBottom)), 30.0f, Color3B::WHITE, 150);
    box->setAnchorPoint(Vec2::ZERO);
    box->setPosition(L, boxBottom);
    _detail->addChild(box);
    _descRect = Rect(L + 40.0f, boxBottom + 26.0f, R - L - 80.0f, std::max(10.0f, boxTop - boxBottom - 52.0f));
    auto* clip = ClippingRectangleNode::create(_descRect);
    _detail->addChild(clip);
    _desc = Label::createWithTTF("", oui::kFontBody, 44.0f, Size(_descRect.size.width, 0.0f));
    _desc->setAnchorPoint(Vec2(0.0f, 1.0f));
    _desc->setLineSpacing(8.0f);
    clip->addChild(_desc);
}

void UserLevelsScreen::buildInput()
{
    auto keys = EventListenerKeyboard::create();
    keys->onKeyPressed = [this](EventKeyboard::KeyCode key, Event*) {
        using K = EventKeyboard::KeyCode;
        if (!isRunning() || oui::modalOpen()) return;
        const int last = static_cast<int>(_levels.size()) - 1;
        int sel = -1;
        for (int i = 0; i <= last; ++i)
        {
            if (_levels[i]->id_x() == screenState().selectedId[screenState().tab]) sel = i;
        }
        const int page = std::max(1, static_cast<int>(_rowsRect.size.height / _rowH));
        switch (key)
        {
        case K::KEY_ESCAPE: goBack(); break;  // also Android BACK (same key code)
        case K::KEY_ENTER:
        case K::KEY_KP_ENTER: playSelected(); break;
        case K::KEY_DELETE: deleteSelected(); break;
        case K::KEY_TAB: setTab(1 - screenState().tab); break;
        case K::KEY_UP_ARROW: if (last >= 0) select(std::max(0, sel - 1), true); break;
        case K::KEY_DOWN_ARROW: if (last >= 0) select(std::min(last, sel + 1), true); break;
        case K::KEY_PG_UP: if (last >= 0) select(std::max(0, sel - page), true); break;
        case K::KEY_PG_DOWN: if (last >= 0) select(std::min(last, sel + page), true); break;
        default: break;
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keys, this);

    auto mouse = EventListenerMouse::create();
    mouse->onMouseScroll = [this](EventMouse* e) {
        if (oui::modalOpen()) return;
        const Vec2 p(e->getCursorX(), e->getCursorY());
        if (Rect(_rowsRect.origin.x, _rowsRect.origin.y, _rowsRect.size.width + 50.0f, _rowsRect.size.height).containsPoint(p))
        {
            setListOffset(screenState().offset[screenState().tab] + e->getScrollY() * _rowH);
        }
        else if (_detail->isVisible() && _descRect.containsPoint(p))
        {
            setDescriptionOffset(_descOffset + e->getScrollY() * 110.0f);
        }
    };
    mouse->onMouseMove = [this](EventMouse* e) {
        const int row = (_drag == Drag::None && !oui::modalOpen()) ? rowAt(Vec2(e->getCursorX(), e->getCursorY())) : -1;
        if (row != _hoverRow)
        {
            _hoverRow = row;
            refreshRows();
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, this);

    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (oui::modalOpen()) return false;
        const Vec2 p = t->getLocation();
        _touchStart = p;
        if (_rowsRect.containsPoint(p) && !_levels.empty())
        {
            _drag = Drag::ListPending;
            _dragStartOffset = screenState().offset[screenState().tab];
            return true;
        }
        if (_detail->isVisible() && _descRect.containsPoint(p))
        {
            _drag = Drag::Description;
            _dragStartOffset = _descOffset;
            return true;
        }
        return false;
    };
    touch->onTouchMoved = [this](Touch* t, Event*) {
        const Vec2 p = t->getLocation();
        if (_drag == Drag::ListPending && std::fabs(p.y - _touchStart.y) > 22.0f)
        {
            _drag = Drag::List;
            _hoverRow = -1;
        }
        if (_drag == Drag::List) setListOffset(_dragStartOffset + (p.y - _touchStart.y));
        else if (_drag == Drag::Description) setDescriptionOffset(_dragStartOffset + (p.y - _touchStart.y));
    };
    touch->onTouchEnded = [this](Touch* t, Event*) {
        const Drag drag = _drag;
        _drag = Drag::None;
        if (drag != Drag::ListPending) return;
        const int row = rowAt(t->getLocation());
        if (row < 0) return;
        const double now = utils::gettime();
        const bool dbl = row == _lastClickRow && now - _lastClickTime < 0.4;
        _lastClickRow = row;
        _lastClickTime = now;
        select(row, false);
        if (dbl) playSelected();
    };
    touch->onTouchCancelled = [this](Touch*, Event*) { _drag = Drag::None; };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
}

// ---- state -----------------------------------------------------------------------------------

void UserLevelsScreen::reload()
{
    LevelStore* store = LevelStore::getInstance();
    for (int i = 0; i < 2; ++i)
    {
        _counts[i] = static_cast<int>(store->levelsSortedById(kChapters[i]).size());
    }
    _levels.clear();
    // Newest first: id_x grows with every save / import.
    Vector<LevelMO*> sorted = store->levelsSortedById(kChapters[screenState().tab]);
    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) _levels.push_back(*it);
    ScreenState& st = screenState();
    if (!selectedLevel() && !_levels.empty()) st.selectedId[st.tab] = _levels[0]->id_x();
    if (_levels.empty()) st.selectedId[st.tab] = -1;
    refreshTabs();
    refreshList();
    refreshDetail();
}

void UserLevelsScreen::setTab(int tab)
{
    ScreenState& st = screenState();
    if (tab == st.tab) return;
    st.tab = tab;
    _hoverRow = -1;
    reload();
    setListOffset(st.offset[tab]);
}

LevelMO* UserLevelsScreen::selectedLevel() const
{
    const ScreenState& st = screenState();
    for (const auto& l : _levels)
    {
        if (l->id_x() == st.selectedId[st.tab]) return l.get();
    }
    return nullptr;
}

void UserLevelsScreen::select(int index, bool scrollIntoView)
{
    if (index < 0 || index >= static_cast<int>(_levels.size())) return;
    ScreenState& st = screenState();
    if (_levels[index]->id_x() != st.selectedId[st.tab])
    {
        st.selectedId[st.tab] = _levels[index]->id_x();
        refreshDetail();
    }
    if (scrollIntoView)
    {
        const float top = index * _rowH;
        if (top < st.offset[st.tab]) setListOffset(top);
        else if (top + _rowH > st.offset[st.tab] + _rowsRect.size.height) setListOffset(top + _rowH - _rowsRect.size.height);
    }
    refreshRows();
}

// ---- view --------------------------------------------------------------------------------------

void UserLevelsScreen::refreshTabs()
{
    const char* names[] = {"My Levels", "Received"};
    for (int i = 0; i < 2; ++i)
    {
        _tabs[i]->setStyle(i == screenState().tab ? oui::Button::window("blue") : oui::Button::ghost());
        _tabs[i]->setText(StringUtils::format("%s  %d", names[i], _counts[i]));
    }
}

void UserLevelsScreen::refreshList()
{
    _listMessage->removeAllChildren();
    for (Row& r : _rows) r.bound = -1;
    const bool empty = _levels.empty();
    _rowsNode->setVisible(!empty);
    if (empty)
    {
        const bool received = screenState().tab == 1;
        Label* heading = makeLabel(received ? "Nothing received yet" : "No levels yet", oui::kFontHeading, 84.0f,
                                   Color3B::WHITE, Vec2(0.5f, 0.5f));
        heading->enableShadow(Color4B(0, 0, 0, 110), Size(0.0f, -6.0f));
        heading->setPosition(0.0f, 160.0f);
        _listMessage->addChild(heading);
        Label* body = Label::createWithTTF(
            received ? "When a friend sends you a level with Send to Nearby, it shows up here. Imported "
                       ".happywheels files land here too."
                     : "Build one in the editor, or receive one from a friend.",
            oui::kFontBody, 48.0f, Size(_rowsRect.size.width - 240.0f, 0.0f), TextHAlignment::CENTER);
        body->setColor(oui::kTextDim);
        body->setAnchorPoint(Vec2(0.5f, 1.0f));
        body->setLineSpacing(6.0f);
        body->setPosition(0.0f, 70.0f);
        _listMessage->addChild(body);
        const float by = 70.0f - body->getContentSize().height - 140.0f;
        auto* receiveBtn = oui::Button::create("Receive", Size(460.0f, 150.0f), oui::Button::window("pink"), 60.0f);
        auto* newBtn = oui::Button::create("New Level", Size(460.0f, 150.0f), oui::Button::window("blue"), 60.0f);
        receiveBtn->setCallback([this]() { receive(); });
        newBtn->setCallback([this]() { newLevel(); });
        if (received)
        {
            receiveBtn->setPosition(0.0f, by);
            _listMessage->addChild(receiveBtn);
        }
        else
        {
            newBtn->setPosition(-250.0f, by);
            receiveBtn->setPosition(250.0f, by);
            _listMessage->addChild(newBtn);
            _listMessage->addChild(receiveBtn);
        }
    }
    setListOffset(screenState().offset[screenState().tab]);
}

float UserLevelsScreen::maxListOffset() const
{
    return std::max(0.0f, _levels.size() * _rowH - _rowsRect.size.height);
}

void UserLevelsScreen::setListOffset(float offset)
{
    ScreenState& st = screenState();
    st.offset[st.tab] = std::max(0.0f, std::min(offset, maxListOffset()));
    refreshRows();
}

int UserLevelsScreen::rowAt(const Vec2& world) const
{
    if (!_rowsRect.containsPoint(world) || !_rowsNode->isVisible()) return -1;
    const float fromTop = _rowsRect.getMaxY() - world.y + screenState().offset[screenState().tab];
    const int index = static_cast<int>(std::floor(fromTop / _rowH));
    if (fromTop - index * _rowH > _rowH - 18.0f) return -1;
    return (index >= 0 && index < static_cast<int>(_levels.size())) ? index : -1;
}

void UserLevelsScreen::refreshRows()
{
    const ScreenState& st = screenState();
    const int count = static_cast<int>(_levels.size());
    const float offset = st.offset[st.tab];
    const int first = std::max(0, static_cast<int>(std::floor(offset / _rowH)));
    const int last = std::min(count - 1, static_cast<int>(std::floor((offset + _rowsRect.size.height) / _rowH)));
    const int pool = static_cast<int>(_rows.size());
    for (Row& r : _rows) r.node->setVisible(false);
    for (int k = first; k <= last; ++k)
    {
        Row& r = _rows[k % pool];
        if (r.bound != k) bindRow(r, k);
        const bool selected = _levels[k]->id_x() == st.selectedId[st.tab];
        r.selectedBg->setVisible(selected);
        r.bg->setVisible(!selected);
        r.bg->setOpacity(k == _hoverRow ? 150 : 100);
        r.sub->setColor(selected ? kRowSelectedSub : oui::kTextDim);
        const float yTop = _rowsRect.getMaxY() - (k * _rowH - offset);
        r.node->setPosition(_rowsRect.origin.x, yTop - _rowH);
        r.node->setVisible(true);
    }
    const float total = count * _rowH;
    const float trackH = _rowsRect.size.height - 40.0f;
    const bool scrollable = total > _rowsRect.size.height && _rowsNode->isVisible();
    _scrollThumb->setVisible(scrollable);
    if (scrollable)
    {
        const float thumbH = std::max(90.0f, trackH * _rowsRect.size.height / total);
        const float f = maxListOffset() > 0.0f ? offset / maxListOffset() : 0.0f;
        _scrollThumb->setContentSize(Size(14.0f, thumbH));
        _scrollThumb->setPosition(_rowsRect.getMaxX() + 14.0f, _rowsRect.getMaxY() - 20.0f - f * (trackH - thumbH));
    }
}

void UserLevelsScreen::setPortrait(Sprite* sprite, int character, float fitHeight, float fitWidth)
{
    Texture2D* t = Director::getInstance()->getTextureCache()->addImage(oui::characterPortrait(character));
    if (!t)
    {
        sprite->setVisible(false);
        return;
    }
    sprite->setVisible(true);
    sprite->setTexture(t);
    sprite->setTextureRect(Rect(Vec2::ZERO, t->getContentSize()));
    const Size s = t->getContentSize();
    sprite->setScale(std::min(fitHeight / s.height, fitWidth / s.width));
}

void UserLevelsScreen::bindRow(Row& r, int index)
{
    LevelMO* l = _levels[index].get();
    r.bound = index;
    const int c = shownCharacter(l);
    setPortrait(r.portrait, c, 176.0f, 170.0f);
    const std::string tag = oui::characterTag(c);
    r.tagNode->setVisible(!tag.empty());
    if (!tag.empty()) setPillText(r.tagNode, r.tag, tag);
    const float maxW = _rowsRect.size.width - 244.0f - 50.0f;
    oui::setEllipsized(r.name, levelName(l), maxW);
    std::string sub = l->force_character() ? oui::characterName(l->playable_character()) : "Any character";
    const std::string edited = formatTime(l->modifiedOn() ? l->modifiedOn() : l->createdOn());
    if (!edited.empty()) sub += "  \xC2\xB7  " + edited;
    oui::setEllipsized(r.sub, sub, maxW);
}

void UserLevelsScreen::refreshDetail()
{
    LevelMO* l = selectedLevel();
    _detail->setVisible(l != nullptr);
    _detailEmpty->setVisible(l == nullptr);
    if (auto* hint = dynamic_cast<Label*>(_detailEmpty->getChildByTag(1)))
    {
        hint->setString(_levels.empty() ? "Your levels show up here" : "Pick a level");
    }
    if (!l) return;

    const int c = shownCharacter(l);
    setPortrait(_detailPortrait, c, 262.0f, 236.0f);
    const std::string tag = oui::characterTag(c);
    _detailTag->setVisible(!tag.empty());
    if (!tag.empty()) setPillText(_detailTag, _detailTagLabel, tag);
    {
        const std::string name = levelName(l);
        const float width = _detailName->getDimensions().width;
        _detailName->setOverflow(Label::Overflow::RESIZE_HEIGHT);
        _detailName->setDimensions(width, 0.0f);
        for (float size : {96.0f, 84.0f, 72.0f, 62.0f})
        {
            TTFConfig cfg = _detailName->getTTFConfig();
            cfg.fontSize = size;
            _detailName->setTTFConfig(cfg);
            _detailName->setString(name);
            if (_detailName->getContentSize().height <= _detailNameMaxH) break;
        }
        if (_detailName->getContentSize().height > _detailNameMaxH)
        {
            _detailName->setDimensions(width, _detailNameMaxH);
            _detailName->setOverflow(Label::Overflow::CLAMP);
        }
    }
    const float statW = _detailRect.getMaxX() - 70.0f - _detailChar->getPositionX();
    oui::setEllipsized(_detailChar,
                       l->force_character() ? oui::characterName(l->playable_character()) : "Player's choice", statW);
    const std::string created = formatTime(l->createdOn());
    const std::string edited = formatTime(l->modifiedOn());
    oui::setEllipsized(_detailCreated, created.empty() ? "\xE2\x80\x94" : created, statW * 0.5f - 20.0f);
    oui::setEllipsized(_detailEdited, edited.empty() ? "\xE2\x80\x94" : edited, statW * 0.5f - 20.0f);
    _detailSource->setString(l->chapter() && l->chapter()->chapterIndex() == LevelStoreChapterImported
                                 ? "Received or imported"
                                 : "Made in the editor");
    const std::string desc = levelui::trimmed(l->comments());
    _desc->setString(desc.empty() ? "No description." : desc);
    _desc->setColor(desc.empty() ? oui::kInkDim : oui::kInk);
    setDescriptionOffset(0.0f);
}

void UserLevelsScreen::setDescriptionOffset(float offset)
{
    const float maxOff = std::max(0.0f, _desc->getContentSize().height - _descRect.size.height);
    _descOffset = std::max(0.0f, std::min(offset, maxOff));
    _desc->setPosition(_descRect.origin.x, _descRect.getMaxY() + _descOffset);
}

// ---- actions (the flows of UserLevelSelectUIView) --------------------------------------------

void UserLevelsScreen::goBack()
{
    Director::getInstance()->replaceScene(TransitionFade::create(
        globals::ui::menuFadeTime, MainMenu::createScene(MenuModeMain, nullptr), Color3B(0, 0, 0)));
}

// -[UserLevelSelectUIView playBtnPressed:] @ios 1001072ac
void UserLevelsScreen::playSelected()
{
    LevelMO* level = selectedLevel();
    if (!level) return;
    LevelSession* session = LevelSession::getInstance();
    session->setChapterIndex(LevelStoreChapterUser);
    session->setLevelDataWithManagedObject(level);
    bool forceCharacter = level->force_character();
    session->setCharacterIndex(level->playable_character());
    session->setVehicleIndex(0);
    session->playLevel(forceCharacter);
}

// -[UserLevelSelectUIView editBtnPressed:] @ios 100107220 (port fix: opens the selected level)
void UserLevelsScreen::editSelected()
{
    LevelMO* level = selectedLevel();
    if (!level) return;
    LevelSession::getInstance()->setLevelDataWithManagedObject(level);
    Director::getInstance()->pushScene(EditorLayer::createSceneWithLevelMO(level));
}

// -[UserLevelSelectUIView newLevelBtnPressed:] @ios 100107078
void UserLevelsScreen::newLevel()
{
    Director::getInstance()->pushScene(EditorLayer::createScene());
}

// -[UserLevelSelectUIView deleteBtnPressed:] @ios 1001070c0 + alertView:clickedButtonAtIndex:
void UserLevelsScreen::deleteSelected()
{
    LevelMO* level = selectedLevel();
    if (!level) return;
    _pendingDelete = level;
    std::string title = Localization::get("DELETE THE LEVEL") + " \xE2\x80\x9C" + levelName(level) + "\xE2\x80\x9D";
    HWWindow* window = levelui::showAlert(kTagDelete, title, Localization::get("THIS CANT BE UNDONE"),
                                          levelui::capitalized(Localization::get("CANCEL")), "Delete", this);
    net::ui::greyButton(window, 0);
}

void UserLevelsScreen::hwWindowButtonPressed(int buttonTag, HWWindow* window)
{
    if (window->getTag() != kTagDelete) return;
    RefPtr<LevelMO> level = _pendingDelete;
    _pendingDelete = nullptr;
    if (levelui::buttonIndex(buttonTag, window) != 1 || !level) return;
    LevelStore* store = LevelStore::getInstance();
    store->deleteObject(level.get());
    store->save(nullptr);   // posts kLevelsChangedNotification -> reload()
    reload();
}

// The editor's SHARE LEVEL (ShareAction .happywheels export) for a saved level.
void UserLevelsScreen::shareSelected()
{
    LevelMO* level = selectedLevel();
    if (!level) return;
    ShareAction::shareLevelDataFile(level->data(), static_cast<unsigned int>(level->playable_character()),
                                    level->force_character(), level->name(), std::string(), level->comments(), this);
}

void UserLevelsScreen::sendSelected()
{
    LevelMO* level = selectedLevel();
    if (level) net::showSendToNearby(net::LevelPackage::fromLevel(level));
}

void UserLevelsScreen::receive()
{
    net::showReceiveLevels();
}
