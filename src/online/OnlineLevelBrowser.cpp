// ONLINE (PC addition): see OnlineLevelBrowser.h.
#include "online/OnlineLevelBrowser.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>

#include "Globals.h"
#include "HWWindow.h"
#include "LevelSession.h"
#include "MainMenu.h"
#include "MenuHelper.h"
#include "Settings.h"
#include "online/OnlinePlay.h"
#include "online/OnlineUi.h"
#include "online/account/BrowserExtras.h"  // ONLINE (PC addition): account / replays
#include "qol/CharacterChoice.h"  // QOL (PC addition): "any character"
#include "net/NearbyPanels.h"  // NET (PC addition)
#include "net/race/RaceSession.h"  // NET (PC addition): ghost race

USING_NS_CC;

namespace online {

namespace {

const float G = 70.0f;                      // layout grid (MainMenu's 70-unit margins)
const Color3B kRowSelectedSub(225, 236, 255);
const Color3B kAuthorBlue(46, 120, 186);    // link colour on the light panel

// Sort dropdown entries (index -> query); then the featured list, then (ONLINE PC addition) the
// logged-in player's Favorites and My Levels (BrowserState::special 1 / 2).
const char* const kSortNames[] = {"Top Rated", "Most Played", "Newest", "Oldest", "Featured", "Favorites", "My Levels"};
const SortBy kSorts[] = {SortBy::Rating, SortBy::Plays, SortBy::Newest, SortBy::Oldest};
const char* const kPeriodNames[] = {"All Time", "This Month", "This Week", "Today"};
const Uploaded kPeriods[] = {Uploaded::Anytime, Uploaded::Month, Uploaded::Week, Uploaded::Today};

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Level names on the site often carry padding spaces ("     Temple Run   ").
std::string displayName(const OnlineLevelInfo& l) {
    const std::string n = trim(l.name);
    return n.empty() ? "(untitled)" : n;
}

// Readable text for an HWApi error.
std::string friendlyError(const std::string& error) {
    if (error.find("network error") != std::string::npos)
        return "Couldn't reach totaljerkface.com.\nCheck your internet connection and try again.";
    if (error.empty()) return "Something went wrong.";
    std::string e = error;
    e[0] = (char)toupper((unsigned char)e[0]);
    return e;
}

Label* makeLabel(const std::string& text, const std::string& font, float size, const Color3B& color,
                 const Vec2& anchor = Vec2(0.0f, 0.5f)) {
    Label* l = Label::createWithTTF(text, font, size);
    l->setColor(color);
    l->setAnchorPoint(anchor);
    return l;
}

// A rounded pill with a label (thumbnail tags, the downloaded badge).
Node* makePill(const std::string& text, float fontSize, const Color3B& bg, GLubyte opacity, Label** labelOut) {
    Node* n = Node::create();
    n->setCascadeOpacityEnabled(true);
    Label* l = makeLabel(text, ui::kFontBodyBold, fontSize, Color3B::WHITE, Vec2(0.5f, 0.5f));
    const Size ls = l->getContentSize();
    const Size size(ls.width + fontSize * 1.2f, fontSize * 1.6f);
    auto* b = ui::roundedRect(size, size.height * 0.5f, bg, opacity);
    b->setAnchorPoint(Vec2::ZERO);
    n->addChild(b, 0, 1);
    l->setPosition(size.width * 0.5f, size.height * 0.5f);
    n->addChild(l, 1, 2);
    n->setContentSize(size);
    n->setAnchorPoint(Vec2(0.5f, 0.5f));
    if (labelOut) *labelOut = l;
    return n;
}

void setPillText(Node* pill, Label* label, const std::string& text) {
    label->setString(text);
    const float fs = label->getTTFConfig().fontSize;
    const Size size(label->getContentSize().width + fs * 1.2f, fs * 1.6f);
    if (auto* b = dynamic_cast<cocos2d::ui::Scale9Sprite*>(pill->getChildByTag(1))) b->setContentSize(size);
    label->setPosition(size.width * 0.5f, size.height * 0.5f);
    pill->setContentSize(size);
}

}  // namespace

// ---- state / scenes ---------------------------------------------------------------------------

BrowserState& OnlineLevelBrowser::state() {
    static BrowserState s;
    return s;
}

Scene* OnlineLevelBrowser::createScene() {
    Scene* scene = Scene::create();
    scene->addChild(OnlineLevelBrowser::create());
    return scene;
}

Scene* OnlineLevelBrowser::sceneForReturnFromLevel() {
    BrowserState& st = state();
    st.playingId = 0;
    if (!st.levelReturn.take()) {
        st.autoPlay = false;
        return nullptr;
    }
    return createScene();
}

void OnlineLevelBrowser::levelStarting(int levelId) {
    BrowserState& st = state();
    st.levelReturn.park();
    st.playingId = levelId;
    st.playingHash = 0;
}

void OnlineLevelBrowser::cancelLevel() {
    BrowserState& st = state();
    st.levelReturn.cancel();
    st.playingId = 0;
}

int OnlineLevelBrowser::nextLevelIndex() {
    const BrowserState& st = state();
    if (!st.levelReturn.pending || st.playingId == 0 || !st.loaded) return -1;
    // Still the level the browser started (not a level received from a nearby player since...).
    LevelSession* session = LevelSession::getInstance();
    if (!session->isUserLevel() || std::hash<std::string>()(session->levelDataXML()) != st.playingHash) return -1;
    for (size_t i = 0; i < st.levels.size(); ++i) {
        if (st.levels[i].id == st.playingId) return i + 1 < st.levels.size() ? (int)i + 1 : -1;
    }
    return -1;
}

bool OnlineLevelBrowser::hasNextLevel() { return nextLevelIndex() >= 0; }

Scene* OnlineLevelBrowser::sceneForNextLevel() {
    const int next = nextLevelIndex();
    if (next < 0) return nullptr;
    BrowserState& st = state();
    // init reads these: the new browser shows the next level and starts it (playSelected).
    st.selected = next;
    st.autoPlay = true;
    return sceneForReturnFromLevel();
}

OnlineLevelBrowser::~OnlineLevelBrowser() {
    HWApi::getInstance()->cancel(_listRequest);
    HWApi::getInstance()->cancel(_downloadRequest);
}

// ---- build ------------------------------------------------------------------------------------

bool OnlineLevelBrowser::init() {
    if (!Layer::init()) return false;
    _vs = Director::getInstance()->getVisibleSize();
    _origin = Director::getInstance()->getVisibleOrigin();
    _top = _origin.y + _vs.height;
    ui::loadAtlases();
    MenuHelper::addBg(this, -10);
    // Darker, like the options screen, so the panels and white text stand out.
    addChild(LayerColor::create(Color4B(10, 8, 22, 120)), -9);

    // ONLINE (PC addition): account / replay additions (online/account/BrowserExtras.h).
    _extras = BrowserExtras::create();
    _extras->showSpecial = [this](int special) { setSpecial(special); };
    addChild(_extras);

    buildHeader();
    buildFilters();
    buildList();
    buildDetail();
    buildInput();

    BrowserState& st = state();
    _field->setText(st.fieldText);
    refreshFilters();
    if (st.loaded) {
        refreshListState();
        setListOffset(st.listOffset);
        refreshDetail();
    } else {
        load();
    }
    if (st.autoPlay) {
        // ONLINE (PC addition): NEXT on a browser level's victory menu (sceneForNextLevel).
        st.autoPlay = false;
        if (st.loaded && st.selected >= 0 && st.selected < (int)st.levels.size()) {
            select(st.selected, true);
            scheduleOnce([this](float) { playSelected(); }, 0.0f, "online_autoplay");
        }
    }
    scheduleUpdate();
    return true;
}

void OnlineLevelBrowser::buildHeader() {
    const float h = 210.0f;
    const float cy = _top - G - h * 0.5f;

    // Back: the menus' round back button.
    auto* back = MenuItemImage::create();
    back->setNormalImage(Sprite::createWithSpriteFrameName("menu_main_back_light.png"));
    back->setSelectedImage(Sprite::createWithSpriteFrameName("menu_main_back_dark.png"));
    back->setCallback([this](Ref*) { goBack(); });
    const float backScale = h / back->getContentSize().height;
    back->setScale(backScale);
    back->setPosition(_origin.x + G + h * 0.5f, cy);
    Menu* menu = Menu::create(back, nullptr);
    menu->setPosition(Vec2::ZERO);
    addChild(menu, 5);

    Label* title = makeLabel("Online Levels", ui::kFontHeading, 124.0f, Color3B::WHITE);
    title->enableShadow(Color4B(0, 0, 0, 110), Size(0.0f, -8.0f));
    title->setPosition(_origin.x + G + h + 56.0f, cy + 6.0f);
    addChild(title, 5);
    const float titleRight = title->getPositionX() + title->getContentSize().width;

    // Search field + "by Name / by Author", right-aligned.
    const float fieldH = 156.0f;
    const float byW = 470.0f;
    _searchBy = ui::Dropdown::create({"by Name", "by Author"}, Size(byW, fieldH), ui::Button::window("blue"), 52.0f);
    _searchBy->setPosition(_origin.x + _vs.width - G - byW * 0.5f, cy);
    _searchBy->onSelect = [this](int index) {
        state().searchBy = index == 1 ? SearchBy::Author : SearchBy::Name;
        _field->setPlaceholder(index == 1 ? "Search by author..." : "Search levels...");
        if (!trim(_field->text()).empty()) submitSearch();
        _field->focus();
    };
    addChild(_searchBy, 5);

    const float fieldRight = _origin.x + _vs.width - G - byW - 50.0f;
    // ONLINE (PC addition): the account button between the title and the search field.
    const float accountW = BrowserExtras::accountButtonWidth();
    _extras->buildAccountButton(this, Vec2(titleRight + 70.0f + accountW * 0.5f, cy), fieldH);
    const float fieldLeft = titleRight + 70.0f + accountW + 60.0f;
    // A smaller floor than the field would like, so it doesn't slide over the account button on
    // 4:3 and narrower screens.
    const float fieldW = std::max(400.0f, fieldRight - fieldLeft);
    _field = ui::SearchField::create(Size(fieldW, fieldH), "Search levels...");
    _field->setPosition(fieldRight - fieldW * 0.5f, cy);
    _field->onChange = [this]() { scheduleSearch(); };
    addChild(_field, 5);
}

void OnlineLevelBrowser::buildFilters() {
    const float inner = _vs.width - 3.0f * G;
    const float listW = std::floor(inner * 0.57f);
    const float h = 128.0f;
    const float cy = _top - 330.0f - h * 0.5f;
    float x = _origin.x + G;

    _sort = ui::Dropdown::create(std::vector<std::string>(kSortNames, kSortNames + 7), Size(560.0f, h), ui::Button::window("blue"), 52.0f);
    _sort->setPosition(x + 280.0f, cy);
    _sort->onSelect = [this](int index) { setSortIndex(index); };
    addChild(_sort, 5);
    x += 560.0f + 40.0f;
    _period = ui::Dropdown::create(std::vector<std::string>(kPeriodNames, kPeriodNames + 4), Size(500.0f, h), ui::Button::window("pink"), 52.0f);
    _period->setPosition(x + 250.0f, cy);
    _period->onSelect = [this](int index) { setPeriodIndex(index); };
    addChild(_period, 5);

    // Count + paging at the right end of the list column.
    const float right = _origin.x + G + listW;
    const Size pageBtn(128.0f, h);
    _nextBtn = ui::Button::create("", pageBtn, ui::Button::ghost());
    Sprite* nextIcon = ui::iconSprite("chevron", 56.0f);
    nextIcon->setRotation(-90.0f);
    _nextBtn->setIcon(nextIcon);
    _nextBtn->setPosition(right - pageBtn.width * 0.5f, cy);
    _nextBtn->setCallback([this]() { changePage(+1); });
    addChild(_nextBtn, 5);
    _pageText = makeLabel("", ui::kFontHeading, 50.0f, Color3B::WHITE, Vec2(0.5f, 0.5f));
    _pageText->setPosition(right - pageBtn.width - 110.0f, cy);
    addChild(_pageText, 5);
    _prevBtn = ui::Button::create("", pageBtn, ui::Button::ghost());
    Sprite* prevIcon = ui::iconSprite("chevron", 56.0f);
    prevIcon->setRotation(90.0f);
    _prevBtn->setIcon(prevIcon);
    _prevBtn->setPosition(right - pageBtn.width * 1.5f - 220.0f, cy);
    _prevBtn->setCallback([this]() { changePage(-1); });
    addChild(_prevBtn, 5);
    _countText = makeLabel("", ui::kFontBody, 46.0f, ui::kTextDim, Vec2(1.0f, 0.5f));
    _countText->setPosition(right, cy);
    addChild(_countText, 5);
}

void OnlineLevelBrowser::buildList() {
    const float inner = _vs.width - 3.0f * G;
    const float listW = std::floor(inner * 0.57f);
    const float x0 = _origin.x + G;
    const float top = _top - 520.0f;
    const float bottom = _origin.y + G;
    const float scrollW = 34.0f;
    _rowsRect = Rect(x0, bottom, listW - scrollW, top - bottom);

    auto* clip = ClippingRectangleNode::create(Rect(x0 - 10.0f, bottom, listW + 20.0f, top - bottom));
    addChild(clip, 2);
    _rowsNode = Node::create();
    clip->addChild(_rowsNode);

    const int poolSize = (int)std::ceil(_rowsRect.size.height / _rowH) + 2;
    const float rw = _rowsRect.size.width;
    const float rh = _rowH - 18.0f;   // visible row height (gap below)
    const float midY = 18.0f + rh * 0.5f;
    for (int i = 0; i < poolSize; ++i) {
        Row r;
        r.node = Node::create();
        r.node->setContentSize(Size(rw, _rowH));
        _rowsNode->addChild(r.node);
        r.node->setName("ow_focus");  // PAD (PC addition): a controller focus target (input/MenuFocus.h)
        // Unselected: a dark bar like the options screen; selected: the main menu's blue button.
        r.bg = ui::roundedRect(Size(rw, rh), 26.0f, Color3B(0, 0, 0), 100);
        r.bg->setAnchorPoint(Vec2::ZERO);
        r.bg->setPosition(0.0f, 18.0f);
        r.node->addChild(r.bg);
        r.selectedBg = ui::frameSprite("menu_main_btn_blue_normal.png", Size(rw, rh), Rect(0.25f, 0.25f, 0.5f, 0.5f));
        r.selectedBg->setAnchorPoint(Vec2::ZERO);
        r.selectedBg->setPosition(0.0f, 18.0f);
        r.node->addChild(r.selectedBg);

        r.portrait = Sprite::create();
        r.portrait->setPosition(40.0f + 80.0f, midY);
        r.node->addChild(r.portrait);
        r.tagNode = makePill("ANY", 28.0f, Color3B(40, 38, 62), 235, &r.tag);
        r.tagNode->setPosition(40.0f + 80.0f, 18.0f + 38.0f);
        r.node->addChild(r.tagNode);

        r.name = makeLabel("", ui::kFontHeading, 64.0f, Color3B::WHITE);
        r.name->setPosition(244.0f, midY + 30.0f);
        r.node->addChild(r.name);
        r.author = makeLabel("", ui::kFontBody, 44.0f, ui::kTextDim);
        r.author->setPosition(246.0f, midY - 48.0f);
        r.node->addChild(r.author);

        r.rating = makeLabel("", ui::kFontBodyBold, 44.0f, Color3B::WHITE, Vec2(1.0f, 0.5f));
        r.rating->setPosition(rw - 50.0f, midY);
        r.node->addChild(r.rating);
        r.stars = ui::StarBar::create(58.0f);
        r.stars->setAnchorPoint(Vec2(1.0f, 0.5f));
        r.stars->setPosition(rw - 50.0f - 120.0f, midY);
        r.node->addChild(r.stars);
        r.node->setVisible(false);
        _rows.push_back(r);
    }

    _scrollThumb = ui::roundedRect(Size(14.0f, 100.0f), 7.0f, Color3B::WHITE, 110);
    _scrollThumb->setAnchorPoint(Vec2(0.0f, 1.0f));
    addChild(_scrollThumb, 3);

    // Centre message (loading / error / empty), rebuilt by refreshListState.
    _listMessage = Node::create();
    _listMessage->setPosition(_rowsRect.getMidX(), _rowsRect.getMidY() + 40.0f);
    addChild(_listMessage, 4);
}

void OnlineLevelBrowser::buildDetail() {
    const float inner = _vs.width - 3.0f * G;
    const float listW = std::floor(inner * 0.57f);
    const float x0 = _origin.x + 2.0f * G + listW;
    const float w = _origin.x + _vs.width - G - x0;
    const float top = _top - 330.0f;
    const float bottom = _origin.y + G;
    _detailRect = Rect(x0, bottom, w, top - bottom);

    Sprite* panel = ui::windowPanel(_detailRect.size);
    panel->setAnchorPoint(Vec2::ZERO);
    panel->setPosition(_detailRect.origin);
    addChild(panel, 1);
    const float strip = ui::windowPanelStrip();

    // Nothing selected: a faint globe and a hint.
    _detailEmpty = Node::create();
    addChild(_detailEmpty, 2);
    Sprite* globe = ui::iconSprite("globe", 380.0f);
    globe->setColor(Color3B(150, 150, 158));
    globe->setOpacity(90);
    globe->setPosition(_detailRect.getMidX(), _detailRect.getMidY() + 80.0f);
    _detailEmpty->addChild(globe);
    Label* hint = makeLabel("Pick a level", ui::kFontHeading, 64.0f, ui::kInkDim, Vec2(0.5f, 0.5f));
    hint->setPosition(_detailRect.getMidX(), _detailRect.getMidY() - 200.0f);
    _detailEmpty->addChild(hint, 0, 1);

    _detail = Node::create();
    addChild(_detail, 2);
    const float P = 70.0f;
    const float L = x0 + P;
    const float R = x0 + w - P;
    float y = top - strip - 46.0f;

    // Level name across the panel (sized to fit two lines in refreshDetail).
    const float nameH = 210.0f;
    _detailName = Label::createWithTTF("", ui::kFontHeading, 92.0f, Size(R - L, 0.0f), TextHAlignment::LEFT,
                                       TextVAlignment::BOTTOM);
    _detailName->setColor(ui::kInk);
    _detailName->setAnchorPoint(Vec2(0.0f, 0.0f));
    _detailName->setPosition(L, y - nameH);
    _detail->addChild(_detailName);
    _detailNameMaxH = nameH;
    y -= nameH + 24.0f;

    // Portrait card; author, rating and stats beside it.
    const Size avatar(250.0f, 280.0f);
    auto* card = ui::roundedRect(avatar, 30.0f, Color3B::WHITE, 255);
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
    ui::Button::Style link;
    link.color = Color3B(60, 60, 70);
    link.opacity = 0;
    link.radius = 20.0f;
    link.textColor = kAuthorBlue;
    _authorBtn = ui::Button::create("", Size(300.0f, 76.0f), link, 52.0f, ui::kFontBodyBold);
    _authorBtn->setAnchorPoint(Vec2(0.0f, 0.5f));
    _authorBtn->setPosition(tx - 16.0f, y - 40.0f);
    _authorBtn->setCallback([this]() {
        const BrowserState& st = state();
        if (st.selected >= 0 && st.selected < (int)st.levels.size()) searchAuthor(trim(st.levels[st.selected].authorName));
    });
    _detail->addChild(_authorBtn);

    const float starsY = y - 132.0f;
    _detailStars = ui::StarBar::create(68.0f, Color3B(176, 176, 182), 255);
    _detailStars->setAnchorPoint(Vec2(0.0f, 0.5f));
    _detailStars->setPosition(tx, starsY);
    _detail->addChild(_detailStars);
    _detailRating = makeLabel("", ui::kFontHeading, 60.0f, ui::kInk);
    _detailRating->setPosition(tx + _detailStars->getContentSize().width + 28.0f, starsY - 2.0f);
    _detail->addChild(_detailRating);
    _detailVotes = makeLabel("", ui::kFontBody, 42.0f, ui::kInkDim);
    _detail->addChild(_detailVotes);

    // Plays / published / character: caption over value; the character column is the widest.
    const float statsW = R - tx;
    const float colX[3] = {tx, tx + statsW * 0.27f, tx + statsW * 0.60f};
    const char* captions[] = {"PLAYS", "PUBLISHED", "CHARACTER"};
    Label** values[] = {&_detailPlays, &_detailDate, &_detailChar};
    for (int i = 0; i < 3; ++i) {
        Label* c = makeLabel(captions[i], ui::kFontBodyBold, 32.0f, ui::kInkDim);
        c->setPosition(colX[i], y - 206.0f);
        _detail->addChild(c);
        *values[i] = makeLabel("", ui::kFontBodyBold, 44.0f, ui::kInk);
        (*values[i])->setPosition(colX[i], y - 252.0f);
        _detail->addChild(*values[i]);
    }
    _statColW = statsW;
    // QOL (PC addition): "any character" - CHANGE plays a level that forces its character through
    // character select (qol/CharacterChoice.h); levels that let the player pick always do.
    _charBtn = ui::Button::create("CHANGE", Size(180.0f, 52.0f), ui::Button::window("blue"), 28.0f, ui::kFontBodyBold);
    _charBtn->setAnchorPoint(Vec2(1.0f, 0.5f));
    _charBtn->setPosition(R, y - 200.0f);
    _charBtn->setCallback([this]() { playSelected(true); });
    _detail->addChild(_charBtn);
    y -= avatar.height + 50.0f;
    // ONLINE (PC addition): RATE + favorite on the author line, the REPLAYS bar below the stats.
    y = _extras->buildDetail(_detail, _authorBtn->getPositionY(), L, R, y);

    // Play button at the bottom, status line above it.
    const float playH = 250.0f;
    _playBtn = ui::Button::create("PLAY", Size(R - L, playH), ui::Button::playButton(), 116.0f);
    SpriteFrame* playFrame = SpriteFrameCache::getInstance()->getSpriteFrameByName("menu_main_icon_play.png");
    if (playFrame) {
        Sprite* icon = Sprite::createWithTexture(playFrame->getTexture(), playFrame->getRect(), playFrame->isRotated());
        icon->setScale(0.62f);
        _playBtn->setIcon(icon);
    }
    _playBtn->setPosition(L + (R - L) * 0.5f, bottom + P + playH * 0.5f);
    _playBtn->setCallback([this]() { playSelected(); });
    _detail->addChild(_playBtn);
    if (openInEditorHandler()) {
        // EDITOR (PC addition): EDIT beside PLAY opens the level in the level editor to remix it.
        const float editW = 470.0f, gap = 30.0f;
        _playBtn->setButtonSize(Size(R - L - editW - gap, playH));
        _playBtn->setPosition(L + (R - L - editW - gap) * 0.5f, bottom + P + playH * 0.5f);
        _editBtn = ui::Button::create("EDIT", Size(editW, playH), ui::Button::chunky("pink"), 96.0f);
        _editBtn->setPosition(R - editW * 0.5f, bottom + P + playH * 0.5f);
        _editBtn->setCallback([this]() { editSelected(); });
        _detail->addChild(_editBtn);
    }
    const float statusY = bottom + P + playH + 56.0f;
    _statusSpinner = ui::createSpinner(56.0f, ui::kInkDim);
    _statusSpinner->setPosition(L + 28.0f, statusY);
    _detail->addChild(_statusSpinner);
    _status = makeLabel("", ui::kFontBody, 42.0f, ui::kInkDim);
    _status->setPosition(L, statusY);
    _detail->addChild(_status);
    Label* badgeLabel = nullptr;
    _cachedBadge = makePill("DOWNLOADED", 30.0f, Color3B(70, 165, 100), 255, &badgeLabel);
    _cachedBadge->setAnchorPoint(Vec2(1.0f, 0.5f));
    _cachedBadge->setPosition(R, statusY);
    _detail->addChild(_cachedBadge);

    // Author's comment between the stats and the status line.
    Label* commentCaption = makeLabel("AUTHOR'S COMMENT", ui::kFontBodyBold, 34.0f, ui::kInkDim);
    commentCaption->setPosition(L, y - 18.0f);
    _detail->addChild(commentCaption);
    // NET (PC addition): SEND TO NEARBY, right of the caption, for levels already downloaded.
    _sendBtn = ui::Button::create("SEND TO NEARBY", Size(400.0f, 72.0f), ui::Button::window("blue"), 34.0f,
                                  ui::kFontBodyBold);
    _sendBtn->setPosition(R - 200.0f, y - 18.0f);
    _sendBtn->setCallback([this]() { sendSelected(); });
    _detail->addChild(_sendBtn);
    // NET (PC addition): RACE (ghost race with nearby players) left of SEND TO NEARBY.
    _raceBtn = ui::Button::create("RACE", Size(220.0f, 72.0f), ui::Button::window("pink"), 34.0f, ui::kFontBodyBold);
    _raceBtn->setPosition(R - 400.0f - 30.0f - 110.0f, y - 18.0f);
    _raceBtn->setCallback([this]() { raceSelected(); });
    _detail->addChild(_raceBtn);
    const float boxTop = y - 56.0f;
    const float boxBottom = statusY + 56.0f;
    auto* box = ui::roundedRect(Size(R - L, boxTop - boxBottom), 30.0f, Color3B::WHITE, 150);
    box->setAnchorPoint(Vec2::ZERO);
    box->setPosition(L, boxBottom);
    _detail->addChild(box);
    _commentRect = Rect(L + 40.0f, boxBottom + 26.0f, R - L - 40.0f - 60.0f, boxTop - boxBottom - 52.0f);
    auto* clip = ClippingRectangleNode::create(_commentRect);
    _detail->addChild(clip);
    _comment = Label::createWithTTF("", ui::kFontBody, 44.0f, Size(_commentRect.size.width, 0.0f));
    _comment->setColor(ui::kInk);
    _comment->setAnchorPoint(Vec2(0.0f, 1.0f));
    _comment->setLineSpacing(8.0f);
    clip->addChild(_comment);
    _commentThumb = ui::roundedRect(Size(12.0f, 60.0f), 6.0f, Color3B(0, 0, 0), 60);
    _commentThumb->setAnchorPoint(Vec2(0.0f, 1.0f));
    _detail->addChild(_commentThumb);
}

void OnlineLevelBrowser::buildInput() {
    auto keys = EventListenerKeyboard::create();
    keys->onKeyPressed = [this](EventKeyboard::KeyCode key, Event*) {
        using K = EventKeyboard::KeyCode;
        if (key == K::KEY_CTRL || key == K::KEY_LEFT_CTRL || key == K::KEY_RIGHT_CTRL) _ctrlDown = true;
        if (!isRunning()) return;
        if (ui::Dropdown::anyOpen()) {
            if (key == K::KEY_ESCAPE) ui::Dropdown::closeAll();
            return;
        }
        if (ui::modalOpen()) return;
        const BrowserState& st = state();
        const int visibleRows = std::max(1, (int)(_rowsRect.size.height / _rowH));
        const int last = (int)st.levels.size() - 1;
        switch (key) {
            case K::KEY_ESCAPE: goBack(); break;
            case K::KEY_ENTER:
            case K::KEY_KP_ENTER: submitSearch(); break;
            case K::KEY_UP_ARROW: if (last >= 0) select(std::max(0, st.selected - 1), true); break;
            case K::KEY_DOWN_ARROW: if (last >= 0) select(std::min(last, st.selected + 1), true); break;
            case K::KEY_PG_UP: if (last >= 0) select(std::max(0, st.selected - visibleRows), true); break;
            case K::KEY_PG_DOWN: if (last >= 0) select(std::min(last, st.selected + visibleRows), true); break;
            case K::KEY_V: if (_ctrlDown) _field->paste(); break;
            default: break;
        }
    };
    keys->onKeyReleased = [this](EventKeyboard::KeyCode key, Event*) {
        using K = EventKeyboard::KeyCode;
        if (key == K::KEY_CTRL || key == K::KEY_LEFT_CTRL || key == K::KEY_RIGHT_CTRL) _ctrlDown = false;
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keys, this);

    auto mouse = EventListenerMouse::create();
    mouse->onMouseScroll = [this](EventMouse* e) {
        if (ui::modalOpen()) return;
        const Vec2 p(e->getCursorX(), e->getCursorY());
        if (Rect(_rowsRect.origin.x, _rowsRect.origin.y, _rowsRect.size.width + 50.0f, _rowsRect.size.height).containsPoint(p)) {
            _velocity = 0.0f;
            setListOffset(state().listOffset + e->getScrollY() * _rowH * 1.0f);
        } else if (_detail->isVisible() && _commentRect.containsPoint(p)) {
            setCommentOffset(_commentOffset + e->getScrollY() * 110.0f);
        }
    };
    mouse->onMouseMove = [this](EventMouse* e) {
        const int row = (_drag == Drag::None && !ui::modalOpen()) ? rowAt(Vec2(e->getCursorX(), e->getCursorY())) : -1;
        if (row != _hoverRow) {
            _hoverRow = row;
            refreshRows(false);
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(mouse, this);

    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        const Vec2 p = t->getLocation();
        _touchStart = p;
        _velocity = 0.0f;
        const Rect track(_rowsRect.getMaxX(), _rowsRect.origin.y, 50.0f, _rowsRect.size.height);
        if (_rowsRect.containsPoint(p) && _rowsNode->isVisible() && !state().levels.empty()) {
            _drag = Drag::ListPending;
            _dragStartOffset = state().listOffset;
        } else if (track.containsPoint(p) && maxListOffset() > 0.0f) {
            _drag = Drag::Scrollbar;
            const float f = (_rowsRect.getMaxY() - p.y) / _rowsRect.size.height;
            setListOffset(f * (maxListOffset() + _rowsRect.size.height) - _rowsRect.size.height * 0.5f);
        } else if (_detail->isVisible() && _commentRect.containsPoint(p)) {
            _drag = Drag::Comment;
            _dragStartOffset = _commentOffset;
        } else {
            return false;
        }
        _lastDragY = p.y;
        _lastDragTime = utils::gettime();
        return true;
    };
    touch->onTouchMoved = [this](Touch* t, Event*) {
        const Vec2 p = t->getLocation();
        if (_drag == Drag::ListPending && std::fabs(p.y - _touchStart.y) > 22.0f) {
            _drag = Drag::List;
            _hoverRow = -1;
        }
        if (_drag == Drag::List) {
            setListOffset(_dragStartOffset + (p.y - _touchStart.y));
            const double now = utils::gettime();
            const float dt = (float)std::max(1e-3, now - _lastDragTime);
            _velocity = 0.6f * _velocity + 0.4f * ((p.y - _lastDragY) / dt);
            _lastDragY = p.y;
            _lastDragTime = now;
        } else if (_drag == Drag::Scrollbar) {
            const float f = (_rowsRect.getMaxY() - p.y) / _rowsRect.size.height;
            setListOffset(f * (maxListOffset() + _rowsRect.size.height) - _rowsRect.size.height * 0.5f);
        } else if (_drag == Drag::Comment) {
            setCommentOffset(_dragStartOffset + (p.y - _touchStart.y));
        }
    };
    touch->onTouchEnded = [this](Touch* t, Event*) {
        const Drag drag = _drag;
        _drag = Drag::None;
        if (drag == Drag::List) {
            if (utils::gettime() - _lastDragTime > 0.08) _velocity = 0.0f;
            return;
        }
        _velocity = 0.0f;
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

void OnlineLevelBrowser::onEnter() {
    Layer::onEnter();
    _field->focus();
}

void OnlineLevelBrowser::onExit() {
    // A level pushed over the browser, or the browser was replaced: never call back into it.
    HWApi::getInstance()->cancel(_listRequest);
    _listRequest = 0;
    if (_loading) {
        _loading = false;
        state().loaded = false;  // reopen reloads
    }
    ui::Dropdown::closeAll();
    _field->detachWithIME();
    state().fieldText = _field->text();
    Layer::onExit();
}

// ---- actions ----------------------------------------------------------------------------------

void OnlineLevelBrowser::goBack() {
    if (_playing) {
        // Cancel a pending download; the player stays in the browser.
        HWApi::getInstance()->cancel(_downloadRequest);
        _downloadRequest = 0;
        _playing = false;
        unschedule("online_convert");
        refreshPlayButton();
        return;
    }
    state().fieldText = _field->text();
    Director::getInstance()->replaceScene(TransitionFade::create(globals::ui::menuFadeTime,
                                                                 MainMenu::createScene(MenuModeMain, nullptr),
                                                                 Color3B(0, 0, 0)));
}

void OnlineLevelBrowser::scheduleSearch() {
    // Search shortly after the player stops typing (Enter searches at once). Requests stay rare:
    // nothing is sent while typing, and an unchanged term sends nothing.
    unschedule("online_search");
    scheduleOnce([this](float) {
        const BrowserState& st = state();
        const std::string term = trim(_field->text());
        const bool same = !st.featured && term == st.query.term &&
                          (term.empty() || st.query.searchBy == st.searchBy);
        if (!same && (term.empty() || StringUtils::getCharacterCountInUTF8String(term) >= 2)) submitSearch();
    }, 0.8f, "online_search");
}

void OnlineLevelBrowser::submitSearch() {
    unschedule("online_search");
    BrowserState& st = state();
    st.fieldText = _field->text();
    const std::string term = trim(st.fieldText);
    if (st.featured) st.featured = false;
    st.special = 0;  // ONLINE (PC addition)
    st.query.term = term;
    st.query.searchBy = term.empty() ? SearchBy::None : st.searchBy;
    st.query.page = 1;
    load();
}

void OnlineLevelBrowser::setSortIndex(int index) {
    BrowserState& st = state();
    if (index >= 5) {
        setSpecial(index - 4);  // ONLINE (PC addition)
        return;
    }
    st.special = 0;
    if (index == 4) {
        st.featured = true;
    } else {
        st.featured = false;
        st.query.sortBy = kSorts[index];
        st.query.page = 1;
    }
    load();
}

// ONLINE (PC addition): the logged-in player's Favorites (1) / My Levels (2).
void OnlineLevelBrowser::setSpecial(int special) {
    RefPtr<OnlineLevelBrowser> self(this);
    const bool now = _extras->requireLogin(special == BrowserExtras::Favorites ? "Log in to see your favorite levels."
                                                                              : "Log in to see your levels.",
                                           [self, special]() {
                                               if (!self->isRunning()) return;
                                               BrowserState& st = state();
                                               st.special = special;
                                               st.featured = false;
                                               self->load();
                                           });
    if (!now) refreshFilters();  // the login panel is open; keep the old choice showing
}

void OnlineLevelBrowser::setPeriodIndex(int index) {
    BrowserState& st = state();
    st.special = 0;  // ONLINE (PC addition)
    st.featured = false;
    st.query.uploaded = kPeriods[index];
    st.query.page = 1;
    load();
}

void OnlineLevelBrowser::searchAuthor(const std::string& author) {
    BrowserState& st = state();
    st.searchBy = SearchBy::Author;
    _field->setText(author);
    st.fieldText = author;
    st.featured = false;
    st.special = 0;  // ONLINE (PC addition)
    st.query.searchBy = SearchBy::Author;
    st.query.term = author;
    st.query.page = 1;
    load();
}

void OnlineLevelBrowser::changePage(int delta) {
    BrowserState& st = state();
    if (st.featured || st.special || _loading) return;
    st.query.page = std::max(1, st.query.page + delta);
    load();
}

void OnlineLevelBrowser::load() {
    unschedule("online_search");
    BrowserState& st = state();
    HWApi* api = HWApi::getInstance();
    api->cancel(_listRequest);
    const int gen = ++_generation;
    _loading = true;
    _error.clear();
    st.loaded = false;
    st.levels.clear();
    st.selected = -1;
    st.listOffset = 0.0f;
    _velocity = 0.0f;
    refreshFilters();
    refreshListState();
    refreshDetail();
    auto done = [this, gen](bool ok, const std::string& error, const std::vector<OnlineLevelInfo>& levels, int page,
                            int perPage) {
        if (gen != _generation) return;
        _listRequest = 0;
        _loading = false;
        BrowserState& s = state();
        std::string err = error;
        // Testing aid: OW_ONLINE_FAKE_ERROR=<text> makes every list request fail with <text>.
        if (const char* fake = std::getenv("OW_ONLINE_FAKE_ERROR")) {
            if (*fake) {
                ok = false;
                err = fake;
            }
        }
        if (!ok) {
            _error = err.empty() ? "unknown error" : err;
        } else {
            s.levels = levels;
            s.perPage = perPage;
            s.loaded = true;
            s.selected = levels.empty() ? -1 : 0;
            if (!s.featured && page > 0) s.query.page = page;
        }
        refreshListState();
        setListOffset(0.0f);
        refreshDetail();
    };
    if (st.special) {
        _listRequest = _extras->loadSpecial(st.special, done);  // ONLINE (PC addition)
        return;
    }
    _listRequest = st.featured ? api->listFeatured(done) : api->listLevels(st.query, done);
}

void OnlineLevelBrowser::playSelected(bool chooseCharacter) {
    BrowserState& st = state();
    if (_playing || st.selected < 0 || st.selected >= (int)st.levels.size()) return;
    const OnlineLevelInfo level = st.levels[st.selected];
    _playing = true;
    _chooseCharacter = chooseCharacter;
    const bool cached = HWApi::getInstance()->isCached(level.id);
    refreshPlayButton();
    _status->setString(cached ? "Loading level..." : "Downloading level...");
    // countPlay: the browser client counts a play when a level is started.
    _downloadRequest = HWApi::getInstance()->downloadLevel(level, true, [this](bool ok, const std::string& error,
                                                                             const std::string& xml) {
        _downloadRequest = 0;
        if (!_playing) return;
        if (!ok) {
            _playing = false;
            refreshPlayButton();
            HWWindow* w = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, false, false);
            w->showAlertMessage("Couldn't download level", friendlyError(error), "OK", "", true);
            return;
        }
        _status->setString("Converting level...");
        // Let the status line draw before the (synchronous) conversion runs.
        scheduleOnce([this, xml](float) { finishPlay(xml); }, 0.05f, "online_convert");
    });
}

// NET (PC addition): the browser-format XML goes as kind "flash"; the receiver converts it like
// finishPlay does. Only offered for cached levels, so this reads the cache (no play is counted).
void OnlineLevelBrowser::sendSelected() {
    const BrowserState& st = state();
    if (_playing || st.selected < 0 || st.selected >= (int)st.levels.size()) return;
    const OnlineLevelInfo level = st.levels[st.selected];
    if (!HWApi::getInstance()->isCached(level.id)) return;
    _field->detachWithIME();
    HWApi::getInstance()->downloadLevel(level, false, [level](bool ok, const std::string&, const std::string& xml) {
        if (!ok) return;
        net::LevelPackage package;
        package.name = level.name;
        package.comments = level.comment;
        package.data = xml;
        package.kind = "flash";
        package.playableCharacter = level.character;
        package.forceCharacter = level.character != 0;
        net::showSendToNearby(package);
    });
}

// NET (PC addition): downloads (without counting a play), converts and hosts a ghost race on it.
void OnlineLevelBrowser::raceSelected() {
    const BrowserState& st = state();
    if (_playing || st.selected < 0 || st.selected >= (int)st.levels.size()) return;
    const OnlineLevelInfo level = st.levels[st.selected];
    _field->detachWithIME();
    HWApi::getInstance()->downloadLevel(level, false, [level](bool ok, const std::string& error, const std::string& xml) {
        if (!ok) {
            log("race: %s", error.c_str());
            return;
        }
        ConversionReport report;
        race::RaceLevel r;
        r.kind = "online";
        r.name = level.name;
        r.xml = FlashLevelConverter::toMobile(xml, &report);
        if (!report.ok) return;
        r.forced = report.forceCharacter;
        r.forcedCharacter = report.character;
        race::RaceSession::get()->hostLevel(r);
    });
}
// EDITOR (PC addition): downloads (without counting a play) and opens the level in the editor.
void OnlineLevelBrowser::editSelected() {
    BrowserState& st = state();
    if (_playing || st.selected < 0 || st.selected >= (int)st.levels.size() || !openInEditorHandler()) return;
    const OnlineLevelInfo level = st.levels[st.selected];
    _playing = true;
    refreshPlayButton();
    _status->setString("Opening in the editor...");
    _downloadRequest = HWApi::getInstance()->downloadLevel(level, false, [this, level](bool ok, const std::string& error,
                                                                                     const std::string& xml) {
        _downloadRequest = 0;
        if (!_playing) return;
        _playing = false;
        refreshPlayButton();
        if (!ok) {
            HWWindow* w = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, false, false);
            w->showAlertMessage("Couldn't download level", friendlyError(error), "OK", "", true);
            return;
        }
        state().fieldText = _field->text();
        openInEditorHandler()(xml, level);
    });
}

void OnlineLevelBrowser::finishPlay(const std::string& xml) {
    if (!_playing) return;
    BrowserState& st = state();
    ConversionReport report;
    std::string error;
    // Remember the browser for the way back before the level scene is pushed.
    st.fieldText = _field->text();
    const bool hasLevel = st.selected >= 0 && st.selected < (int)st.levels.size();
    levelStarting(hasLevel ? st.levels[st.selected].id : 0);
    if (!startConvertedLevel(xml, &report, &error, _chooseCharacter)) {
        cancelLevel();
        _playing = false;
        refreshPlayButton();
        HWWindow* w = Settings::getInstance()->createWindow(HWWindowAppearanceAlert, nullptr, false, false);
        w->showAlertMessage("Couldn't play this level", friendlyError(error), "OK", "", true);
        return;
    }
    // NEXT on its victory menu checks that this level still plays (nextLevelIndex).
    st.playingHash = std::hash<std::string>()(LevelSession::getInstance()->levelDataXML());
    // The converter's warnings (report.warnings) go to the log only: the level just starts.
    for (const std::string& w : report.warnings) log("online: %s", w.c_str());
    // ONLINE (PC addition): runs of this level are recorded as browser replays.
    if (hasLevel) BrowserExtras::levelStarted(st.levels[st.selected]);
    _status->setString("Starting...");
}

// ---- views ------------------------------------------------------------------------------------

void OnlineLevelBrowser::refreshFilters() {
    const BrowserState& st = state();
    int sort = 4;
    if (!st.featured)
        for (int i = 0; i < 4; ++i)
            if (kSorts[i] == st.query.sortBy) sort = i;
    if (st.special) sort = 4 + st.special;  // ONLINE (PC addition)
    _sort->setSelectedIndex(sort);
    for (int i = 0; i < 4; ++i)
        if (kPeriods[i] == st.query.uploaded) _period->setSelectedIndex(i);
    _period->setEnabled(!st.featured && !st.special);
    _searchBy->setSelectedIndex(st.searchBy == SearchBy::Author ? 1 : 0);
    _field->setPlaceholder(st.searchBy == SearchBy::Author ? "Search by author..." : "Search levels...");
}

void OnlineLevelBrowser::refreshListState() {
    const BrowserState& st = state();
    _listMessage->removeAllChildren();
    const bool showList = !_loading && _error.empty() && !st.levels.empty();
    _rowsNode->setVisible(showList);

    auto heading = [this](const std::string& text, float y) {
        Label* l = makeLabel(text, ui::kFontHeading, 84.0f, Color3B::WHITE, Vec2(0.5f, 0.5f));
        l->enableShadow(Color4B(0, 0, 0, 110), Size(0.0f, -6.0f));
        l->setPosition(0.0f, y);
        _listMessage->addChild(l);
    };
    auto body = [this](const std::string& text, float y) {
        Label* l = Label::createWithTTF(text, ui::kFontBody, 48.0f, Size(_rowsRect.size.width - 240.0f, 0.0f),
                                        TextHAlignment::CENTER);
        l->setColor(ui::kTextDim);
        l->setAnchorPoint(Vec2(0.5f, 1.0f));
        l->setLineSpacing(6.0f);
        l->setPosition(0.0f, y);
        _listMessage->addChild(l);
        return l;
    };
    if (_loading) {
        Sprite* spin = ui::createSpinner(150.0f, Color3B::WHITE);
        spin->setPosition(0.0f, 80.0f);
        _listMessage->addChild(spin);
        body(st.special ? std::string("Loading ") + BrowserExtras::specialName(st.special) + "..."
                        : (st.featured ? "Loading featured levels..." : "Loading levels..."), -40.0f);
    } else if (!_error.empty()) {
        heading("Couldn't load levels", 180.0f);
        Label* msg = body(friendlyError(_error), 80.0f);
        auto* retry = ui::Button::create("Retry", Size(440.0f, 150.0f), ui::Button::window("blue"), 60.0f);
        retry->setPosition(0.0f, 80.0f - msg->getContentSize().height - 140.0f);
        retry->setCallback([this]() { load(); });
        _listMessage->addChild(retry);
    } else if (st.loaded && st.levels.empty()) {
        heading("No levels found", 120.0f);
        std::string hint;
        if (st.special)
            hint = _extras->emptyHint(st.special);  // ONLINE (PC addition)
        else if (st.featured)
            hint = "There are no featured levels right now.";
        else if (st.query.searchBy == SearchBy::Name && !st.query.term.empty())
            hint = "Check the spelling, or search by author instead.";
        else if (st.query.searchBy == SearchBy::Author && !st.query.term.empty())
            hint = "Check the spelling, or search by level name instead.";
        else
            hint = "Nothing was uploaded in this period.";
        if (!st.featured && !st.special && st.query.uploaded != Uploaded::Anytime) hint += "\nTry All Time for older levels.";
        body(hint, 20.0f);
    }

    // Count + paging.
    const int n = (int)st.levels.size();
    std::string count;
    if (showList) {
        count = ui::formatThousands(n) + (n == 1 ? " level" : " levels");
        if (st.query.searchBy == SearchBy::Author && !st.query.term.empty() && !st.featured && !st.special) count += " by this author";
        if (st.special == BrowserExtras::Favorites) count += " in your favorites";  // ONLINE (PC addition)
    }
    const bool full = st.perPage > 0 && n >= st.perPage;
    const bool paging = !st.featured && !st.special && !_loading && _error.empty() && (st.query.page > 1 || full);
    _prevBtn->setVisible(paging);
    _nextBtn->setVisible(paging);
    _pageText->setVisible(paging);
    _prevBtn->setEnabled(st.query.page > 1);
    _nextBtn->setEnabled(full);
    _pageText->setString("Page " + std::to_string(st.query.page));
    _countText->setString(count);
    _countText->setPositionX(paging ? _prevBtn->getPositionX() - 64.0f - 40.0f : _rowsRect.getMaxX() + 34.0f);
    refreshRows(true);
}

float OnlineLevelBrowser::maxListOffset() const {
    return std::max(0.0f, state().levels.size() * _rowH - _rowsRect.size.height);
}

void OnlineLevelBrowser::setListOffset(float offset) {
    BrowserState& st = state();
    st.listOffset = std::max(0.0f, std::min(offset, maxListOffset()));
    refreshRows(false);
}

int OnlineLevelBrowser::rowAt(const Vec2& world) const {
    if (!_rowsRect.containsPoint(world) || !_rowsNode->isVisible()) return -1;
    const BrowserState& st = state();
    const float fromTop = _rowsRect.getMaxY() - world.y + st.listOffset;
    const int index = (int)std::floor(fromTop / _rowH);
    if (fromTop - index * _rowH > _rowH - 18.0f) return -1;  // the gap between rows
    return (index >= 0 && index < (int)st.levels.size()) ? index : -1;
}

void OnlineLevelBrowser::refreshRows(bool rebindAll) {
    const BrowserState& st = state();
    const int count = (int)st.levels.size();
    const float offset = st.listOffset;
    const int first = std::max(0, (int)std::floor(offset / _rowH));
    const int last = std::min(count - 1, (int)std::floor((offset + _rowsRect.size.height) / _rowH));
    const int pool = (int)_rows.size();
    for (Row& r : _rows) r.node->setVisible(false);
    for (int k = first; k <= last; ++k) {
        Row& r = _rows[k % pool];
        if (rebindAll || r.bound != k) bindRow(r, k);
        styleRow(r, k);
        const float yTop = _rowsRect.getMaxY() - (k * _rowH - offset);
        r.node->setPosition(_rowsRect.origin.x, yTop - _rowH);
        r.node->setVisible(true);
    }
    if (rebindAll)
        for (int i = 0; i < pool; ++i)
            if (!_rows[i].node->isVisible()) _rows[i].bound = -1;

    const float total = count * _rowH;
    const float trackH = _rowsRect.size.height - 40.0f;
    const bool scrollable = total > _rowsRect.size.height && _rowsNode->isVisible();
    _scrollThumb->setVisible(scrollable);
    if (scrollable) {
        const float thumbH = std::max(90.0f, trackH * _rowsRect.size.height / total);
        const float f = maxListOffset() > 0.0f ? offset / maxListOffset() : 0.0f;
        _scrollThumb->setContentSize(Size(14.0f, thumbH));
        _scrollThumb->setPosition(_rowsRect.getMaxX() + 14.0f, _rowsRect.getMaxY() - 20.0f - f * (trackH - thumbH));
    }
}

void OnlineLevelBrowser::setPortrait(Sprite* sprite, int character, float fitHeight, float fitWidth) {
    Texture2D* t = Director::getInstance()->getTextureCache()->addImage(ui::characterPortrait(character));
    if (!t) {
        sprite->setVisible(false);
        return;
    }
    sprite->setVisible(true);
    sprite->setTexture(t);
    sprite->setTextureRect(Rect(Vec2::ZERO, t->getContentSize()));
    const Size s = t->getContentSize();
    sprite->setScale(std::min(fitHeight / s.height, fitWidth / s.width));
    sprite->setOpacity(ui::characterOnMobile(character) ? 255 : 170);
}

void OnlineLevelBrowser::bindRow(Row& r, int index) {
    const OnlineLevelInfo& l = state().levels[index];
    r.bound = index;
    setPortrait(r.portrait, l.character, 176.0f, 170.0f);
    const std::string tag = ui::characterTag(l.character);
    r.tagNode->setVisible(!tag.empty());
    if (!tag.empty()) setPillText(r.tagNode, r.tag, tag);

    r.rating->setString(ui::formatRating(l.rating));
    r.stars->setRating(l.rating);
    r.stars->setPositionX(r.rating->getPositionX() - 112.0f - 22.0f);
    const float rightBlock = r.stars->getContentSize().width + 112.0f + 22.0f + 50.0f + 50.0f;
    const float maxW = _rowsRect.size.width - 244.0f - rightBlock;
    ui::setEllipsized(r.name, displayName(l), maxW);
    ui::setEllipsized(r.author, "by " + trim(l.authorName), maxW);
}

void OnlineLevelBrowser::styleRow(Row& r, int index) {
    const BrowserState& st = state();
    const bool selected = index == st.selected;
    const bool hover = index == _hoverRow;
    r.selectedBg->setVisible(selected);
    r.bg->setVisible(!selected);
    r.bg->setOpacity(hover ? 150 : 100);
    r.author->setColor(selected ? kRowSelectedSub : ui::kTextDim);
    r.rating->setColor(selected ? Color3B::WHITE : Color3B(220, 222, 235));
}

void OnlineLevelBrowser::select(int index, bool scrollIntoView) {
    BrowserState& st = state();
    if (index < 0 || index >= (int)st.levels.size()) return;
    if (_playing) return;
    if (index != st.selected) {
        st.selected = index;
        refreshDetail();
    }
    if (scrollIntoView) {
        const float top = index * _rowH;
        if (top < st.listOffset) setListOffset(top);
        else if (top + _rowH > st.listOffset + _rowsRect.size.height) setListOffset(top + _rowH - _rowsRect.size.height);
    }
    refreshRows(false);
}

void OnlineLevelBrowser::refreshDetail() {
    const BrowserState& st = state();
    const bool has = st.selected >= 0 && st.selected < (int)st.levels.size() && !_loading;
    _detail->setVisible(has);
    _detailEmpty->setVisible(!has);
    if (Node* hint = _detailEmpty->getChildByTag(1)) hint->setVisible(!st.levels.empty());
    _extras->onSelect(has ? &st.levels[st.selected] : nullptr);  // ONLINE (PC addition)
    if (!has) return;
    const OnlineLevelInfo& l = st.levels[st.selected];

    setPortrait(_detailPortrait, l.character, 262.0f, 236.0f);
    _detailTag->setVisible(!ui::characterOnMobile(l.character));
    setPillText(_detailTag, _detailTagLabel, ui::characterTag(l.character));

    {
        // Largest of a few sizes that fits the space above the author; clamp beyond that.
        const std::string name = displayName(l);
        const float width = _detailName->getDimensions().width;
        _detailName->setOverflow(Label::Overflow::RESIZE_HEIGHT);
        _detailName->setDimensions(width, 0.0f);
        for (float size : {96.0f, 84.0f, 72.0f, 62.0f}) {
            TTFConfig cfg = _detailName->getTTFConfig();
            cfg.fontSize = size;
            _detailName->setTTFConfig(cfg);
            _detailName->setString(name);
            if (_detailName->getContentSize().height <= _detailNameMaxH) break;
        }
        if (_detailName->getContentSize().height > _detailNameMaxH) {
            _detailName->setDimensions(width, _detailNameMaxH);
            _detailName->setOverflow(Label::Overflow::CLAMP);
        }
    }
    {
        const std::string author = "by " + trim(l.authorName);
        const float maxW = _detailRect.getMaxX() - 70.0f - _authorBtn->getPositionX() - 32.0f -
                           BrowserExtras::authorLineReserve();  // ONLINE (PC addition): rate + heart
        ui::setEllipsized(_authorBtn->label(), author, maxW);
        _authorBtn->setButtonSize(Size(_authorBtn->label()->getContentSize().width + 32.0f, 76.0f));
    }

    _detailStars->setRating(l.rating);
    _detailRating->setString(ui::formatRating(l.rating));
    _detailVotes->setString(ui::formatThousands(l.votes) + (l.votes == 1 ? " vote" : " votes"));
    _detailVotes->setPosition(_detailRating->getPositionX() + _detailRating->getContentSize().width + 26.0f,
                              _detailRating->getPositionY() - 2.0f);
    ui::setEllipsized(_detailPlays, ui::formatThousands(l.plays), _statColW * 0.27f - 24.0f);
    ui::setEllipsized(_detailDate, ui::formatDate(l.published), _statColW * 0.33f - 24.0f);
    std::string ch = ui::characterName(l.character);
    if (l.character != 0 && !ui::characterOnMobile(l.character)) ch += "*";
    ui::setEllipsized(_detailChar, ch, _statColW * 0.40f);

    std::string comment = trim(l.comment);
    if (comment.empty()) {
        _comment->setString("The author didn't leave a comment.");
        _comment->setColor(ui::kInkDim);
    } else {
        // \r\n and lone \r from the site
        std::string c;
        for (size_t i = 0; i < comment.size(); ++i) {
            if (comment[i] == '\r') {
                if (i + 1 < comment.size() && comment[i + 1] == '\n') continue;
                c += '\n';
            } else {
                c += comment[i];
            }
        }
        // At most one blank line in a row.
        size_t pos;
        while ((pos = c.find("\n\n\n")) != std::string::npos) c.erase(pos, 1);
        _comment->setString(c);
        _comment->setColor(ui::kInk);
    }
    setCommentOffset(0.0f);
    refreshPlayButton();
}

void OnlineLevelBrowser::setCommentOffset(float offset) {
    const float textH = _comment->getContentSize().height;
    const float maxOff = std::max(0.0f, textH - _commentRect.size.height);
    _commentOffset = std::max(0.0f, std::min(offset, maxOff));
    _comment->setPosition(_commentRect.origin.x, _commentRect.getMaxY() + _commentOffset);
    _commentThumb->setVisible(maxOff > 0.0f);
    if (maxOff > 0.0f) {
        const float trackH = _commentRect.size.height;
        const float thumbH = std::max(50.0f, trackH * _commentRect.size.height / textH);
        _commentThumb->setContentSize(Size(12.0f, thumbH));
        _commentThumb->setPosition(_commentRect.getMaxX() + 24.0f,
                                   _commentRect.getMaxY() - (_commentOffset / maxOff) * (trackH - thumbH));
    }
}

void OnlineLevelBrowser::refreshPlayButton() {
    const BrowserState& st = state();
    const bool has = st.selected >= 0 && st.selected < (int)st.levels.size();
    _playBtn->setEnabled(has && !_playing);
    if (_editBtn) _editBtn->setEnabled(has && !_playing);  // EDITOR (PC addition)
    if (_charBtn)  // QOL (PC addition)
        _charBtn->setVisible(has && !_playing && st.levels[st.selected].character != 0 &&
                             qol::anyCharacterOnForcedLevels());
    _statusSpinner->setVisible(_playing);
    _status->setPositionX(_detailRect.origin.x + 70.0f + (_playing ? 84.0f : 0.0f));
    _cachedBadge->setVisible(has && !_playing && HWApi::getInstance()->isCached(st.levels[st.selected].id));
    if (_sendBtn) _sendBtn->setVisible(_cachedBadge->isVisible());  // NET (PC addition)
    if (_raceBtn) {  // NET (PC addition): RACE, left of SEND TO NEARBY when that one shows
        _raceBtn->setVisible(has && !_playing);
        const float right = _sendBtn->getPositionX() + 200.0f;
        _raceBtn->setPositionX(_sendBtn->isVisible() ? right - 540.0f : right - 110.0f);
    }
    if (!_playing && has) {
        const OnlineLevelInfo& l = st.levels[st.selected];
        std::string s = "Level " + std::to_string(l.id);
        if (l.character != 0 && !ui::characterOnMobile(l.character)) s += "  \xC2\xB7  * not in this version";
        _status->setString(s);
    }
}

void OnlineLevelBrowser::update(float dt) {
    if (_drag == Drag::None && std::fabs(_velocity) > 20.0f) {
        const float before = state().listOffset;
        setListOffset(before + _velocity * dt);
        _velocity *= std::pow(0.03f, dt);
        if (state().listOffset == before) _velocity = 0.0f;
    }
}

}  // namespace online
