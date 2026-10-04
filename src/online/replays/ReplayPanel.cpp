// ONLINE (PC addition): see ReplayPanel.h.
#include "online/replays/ReplayPanel.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "cocos2d.h"
#include "online/OnlineLevelBrowser.h"
#include "online/OnlineUi.h"
#include "online/account/AccountPanels.h"
#include "online/account/TjfAccount.h"
#include "online/replays/ReplayApi.h"
#include "online/replays/ReplayRuntime.h"

USING_NS_CC;

namespace online {
namespace replays {

namespace {

const Color3B kError(214, 72, 72);
const char* const kSortNames[] = {"Fastest", "Top Rated", "Newest", "Oldest"};
const ReplaySort kSorts[] = {ReplaySort::Fastest, ReplaySort::Rating, ReplaySort::Newest, ReplaySort::Oldest};
int s_sortIndex = 0;   // remembered between openings

std::map<int, LevelRecord>& records() {
    static std::map<int, LevelRecord> m;
    return m;
}

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

void noteRecord(int levelId, const std::vector<ReplayInfo>& list, bool fastestSort) {
    LevelRecord& r = records()[levelId];
    r.loaded = true;
    r.count = std::max(r.count, (int)list.size());
    for (const ReplayInfo& info : list) {
        if (!info.completed()) continue;
        if (!r.has || info.frames < r.best.frames) {
            r.best = info;
            r.has = true;
        }
        if (fastestSort) break;
    }
}

std::string playerName() {
    account::TjfAccount* a = account::TjfAccount::get();
    return a->loggedIn() && !a->userName().empty() ? a->userName() : std::string("You");
}

}  // namespace

const LevelRecord* cachedRecord(int levelId) {
    auto it = records().find(levelId);
    return it == records().end() ? nullptr : &it->second;
}

RequestId fetchRecord(int levelId, std::function<void(const LevelRecord&)> done) {
    return ReplayApi::get()->listByLevel(
        levelId, ReplaySort::Fastest, 1,
        [levelId, done](bool ok, const std::string&, const std::vector<ReplayInfo>& list, int, int) {
            if (!ok) return;
            noteRecord(levelId, list, true);
            if (done) done(records()[levelId]);
        });
}

// ---- list panel ----------------------------------------------------------------------------------

bool ReplayListPanel::initList(const std::string& title, const std::string& subtitle, bool withSort) {
    if (!initPanel(Size(3300.0f, 1880.0f), title)) return false;
    const float M = 90.0f;
    const float listW = std::floor(_size.width * 0.56f);
    float y = _top + 20.0f;
    if (!subtitle.empty()) {
        Label* sub = tjfui::label("", ui::kFontHeading, 54.0f, ui::kInkDim, Vec2(0.5f, 0.5f));
        ui::setEllipsized(sub, subtitle, _size.width - 600.0f);
        sub->setPosition(_size.width * 0.5f, y - 10.0f);
        _content->addChild(sub);
    }
    y -= 110.0f;

    // Filter row.
    const float rowY = y - 60.0f;
    if (withSort) {
        _sort = ui::Dropdown::create(std::vector<std::string>(kSortNames, kSortNames + 4), Size(560.0f, 112.0f),
                                     ui::Button::window("blue"), 48.0f);
        _sort->setPosition(M + 280.0f, rowY);
        _sort->setSelectedIndex(s_sortIndex);
        _sort->onSelect = [this](int index) {
            s_sortIndex = index;
            reload();
        };
        _content->addChild(_sort);
    }
    _count = tjfui::label("", ui::kFontBody, 42.0f, ui::kInkDim, Vec2(1.0f, 0.5f));
    _count->setPosition(M + listW, rowY);
    _content->addChild(_count);

    const float top = rowY - 90.0f;
    _listRect = Rect(M, M, listW - 30.0f, top - M);
    auto* clip = ClippingRectangleNode::create(Rect(_listRect.origin.x - 10.0f, _listRect.origin.y,
                                                    _listRect.size.width + 20.0f, _listRect.size.height));
    _content->addChild(clip);
    _rowsNode = Node::create();
    clip->addChild(_rowsNode);
    const int pool = (int)std::ceil(_listRect.size.height / _rowH) + 2;
    const float rw = _listRect.size.width, rh = _rowH - 14.0f, mid = 14.0f + rh * 0.5f;
    for (int i = 0; i < pool; ++i) {
        Row r;
        r.node = Node::create();
        _rowsNode->addChild(r.node);
        auto* bg = ui::roundedRect(Size(rw, rh), 24.0f, Color3B::WHITE, 210);
        bg->setAnchorPoint(Vec2::ZERO);
        bg->setPosition(0.0f, 14.0f);
        r.node->addChild(bg);
        r.bg = bg;
        r.selectedBg = ui::roundedRect(Size(rw, rh), 24.0f, ui::kBlue, 255);
        r.selectedBg->setAnchorPoint(Vec2::ZERO);
        r.selectedBg->setPosition(0.0f, 14.0f);
        r.node->addChild(r.selectedBg);
        r.rank = tjfui::label("", ui::kFontHeading, 44.0f, ui::kInkDim, Vec2(0.5f, 0.5f));
        r.rank->setPosition(80.0f, mid);
        r.node->addChild(r.rank);
        r.name = tjfui::label("", ui::kFontBodyBold, 48.0f, ui::kInk);
        r.name->setPosition(170.0f, mid + 26.0f);
        r.node->addChild(r.name);
        r.sub = tjfui::label("", ui::kFontBody, 36.0f, ui::kInkDim);
        r.sub->setPosition(172.0f, mid - 32.0f);
        r.node->addChild(r.sub);
        r.time = tjfui::label("", ui::kFontHeading, 54.0f, ui::kInk, Vec2(1.0f, 0.5f));
        r.time->setPosition(rw - 40.0f, mid + 22.0f);
        r.node->addChild(r.time);
        r.stars = ui::StarBar::create(36.0f, Color3B(176, 176, 182), 255);
        r.stars->setAnchorPoint(Vec2(1.0f, 0.5f));
        r.stars->setPosition(rw - 40.0f, mid - 34.0f);
        r.node->addChild(r.stars);
        r.node->setVisible(false);
        _rows.push_back(r);
    }
    _listMessage = Node::create();
    _listMessage->setPosition(_listRect.getMidX(), _listRect.getMidY() + 60.0f);
    _content->addChild(_listMessage);

    // Row clicks and drag scrolling (on the clip node, above the panel's own swallow-all).
    auto touch = EventListenerTouchOneByOne::create();
    touch->setSwallowTouches(true);
    touch->onTouchBegan = [this](Touch* t, Event*) {
        if (_closing) return false;
        const Vec2 p = _content->convertToNodeSpace(t->getLocation());
        if (!_listRect.containsPoint(p) || _entries.empty()) return false;
        _touchStart = p;
        _dragStart = _offset;
        _dragging = false;
        return true;
    };
    touch->onTouchMoved = [this](Touch* t, Event*) {
        const Vec2 p = _content->convertToNodeSpace(t->getLocation());
        if (!_dragging && std::fabs(p.y - _touchStart.y) > 20.0f) _dragging = true;
        if (_dragging) setOffset(_dragStart + (p.y - _touchStart.y));
    };
    touch->onTouchEnded = [this](Touch* t, Event*) {
        if (_dragging) return;
        const Vec2 p = _content->convertToNodeSpace(t->getLocation());
        const float fromTop = _listRect.getMaxY() - p.y + _offset;
        const int index = (int)std::floor(fromTop / _rowH);
        if (index >= 0 && index < (int)_entries.size()) select(index);
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touch, clip);

    buildDetail();
    return true;
}

void ReplayListPanel::buildDetail() {
    const float M = 90.0f;
    const float x0 = _listRect.getMaxX() + 100.0f;
    const float w = _size.width - M - x0;
    _detailRect = Rect(x0, M, w, _listRect.getMaxY() + 120.0f - M);
    _detail = Node::create();
    _content->addChild(_detail);
    float y = _detailRect.getMaxY();

    _dName = tjfui::label("", ui::kFontHeading, 70.0f, ui::kInk);
    _dName->setPosition(x0, y - 40.0f);
    _detail->addChild(_dName);
    _dTime = tjfui::label("", ui::kFontHeading, 120.0f, ui::kBlue);
    _dTime->setPosition(x0, y - 170.0f);
    _detail->addChild(_dTime);
    _dLine1 = tjfui::label("", ui::kFontBody, 42.0f, ui::kInkDim);
    _dLine1->setPosition(x0, y - 270.0f);
    _detail->addChild(_dLine1);
    _dStars = ui::StarBar::create(60.0f, Color3B(176, 176, 182), 255);
    _dStars->setAnchorPoint(Vec2(0.0f, 0.5f));
    _dStars->setPosition(x0, y - 350.0f);
    _detail->addChild(_dStars);
    _dRating = tjfui::label("", ui::kFontBodyBold, 44.0f, ui::kInk);
    _dRating->setPosition(x0 + _dStars->getContentSize().width + 26.0f, y - 352.0f);
    _detail->addChild(_dRating);
    _dLine2 = tjfui::label("", ui::kFontBody, 40.0f, ui::kInkDim);
    _dLine2->setPosition(x0, y - 430.0f);
    _detail->addChild(_dLine2);

    // Buttons from the bottom: WATCH, then the small action row, then the status line.
    const float watchH = 200.0f;
    _watchBtn = ui::Button::create("WATCH", Size(w, watchH), ui::Button::playButton(), 96.0f);
    _watchBtn->setIcon(tjfui::icon("replay", 96.0f));
    _watchBtn->setPosition(x0 + w * 0.5f, M + watchH * 0.5f);
    _watchBtn->setCallback([this]() { watchSelected(); });
    _detail->addChild(_watchBtn);
    const float actY = M + watchH + 90.0f;
    const float bw = (w - 3 * 30.0f) / 4.0f;
    auto actionBtn = [&](const std::string& text, const std::string& colour, int slot, std::function<void()> cb) {
        auto* b = ui::Button::create(text, Size(bw, 112.0f), ui::Button::window(colour), 44.0f);
        b->setPosition(x0 + bw * 0.5f + slot * (bw + 30.0f), actY);
        b->setCallback(std::move(cb));
        _detail->addChild(b);
        return b;
    };
    _rateBtn = actionBtn("RATE", "pink", 0, [this]() { rateSelected(); });
    _saveBtn = actionBtn("SAVE", "blue", 1, [this]() { saveSelected(); });
    _uploadBtn = actionBtn("UPLOAD", "blue", 2, [this]() { uploadSelected(); });
    _deleteBtn = actionBtn("DELETE", "yellow", 3, [this]() { deleteSelected(); });
    const float statusY = actY + 110.0f;
    _spinner = ui::createSpinner(56.0f, ui::kInkDim);
    _spinner->setPosition(x0 + 28.0f, statusY);
    _spinner->setVisible(false);
    _detail->addChild(_spinner);
    _status = tjfui::label("", ui::kFontBody, 40.0f, ui::kInkDim);
    _status->setPosition(x0, statusY);
    _detail->addChild(_status);

    // Comment / note box between the stats and the status line.
    const float boxTop = y - 490.0f, boxBottom = statusY + 50.0f;
    auto* box = ui::roundedRect(Size(w, std::max(80.0f, boxTop - boxBottom)), 26.0f, Color3B::WHITE, 150);
    box->setAnchorPoint(Vec2::ZERO);
    box->setPosition(x0, boxBottom);
    _detail->addChild(box);
    const Rect inner(x0 + 34.0f, boxBottom + 24.0f, w - 68.0f, boxTop - boxBottom - 48.0f);
    auto* clip = ClippingRectangleNode::create(inner);
    _detail->addChild(clip);
    _dComment = tjfui::textBlock("", ui::kFontBody, 42.0f, ui::kInk, inner.size.width);
    _dComment->setPosition(inner.origin.x, inner.getMaxY());
    clip->addChild(_dComment);
    _dNote = tjfui::textBlock("", ui::kFontBody, 34.0f, ui::kInkDim, inner.size.width);
    _dNote->setAnchorPoint(Vec2(0.0f, 0.0f));
    _dNote->setPosition(inner.origin.x, inner.origin.y);
    clip->addChild(_dNote);
    _detail->setVisible(false);
}

void ReplayListPanel::setStatus(const std::string& text, bool busy) {
    _busy = busy;
    _spinner->setVisible(busy);
    _status->setString(text);
    _status->setPositionX(_detailRect.origin.x + (busy ? 80.0f : 0.0f));
    refreshDetail();
}

void ReplayListPanel::setListMessage(const std::string& text, bool spinner) {
    _listMessage->removeAllChildren();
    if (text.empty()) return;
    if (spinner) {
        Sprite* s = ui::createSpinner(120.0f, ui::kInkDim);
        s->setPosition(0.0f, 90.0f);
        _listMessage->addChild(s);
    }
    Label* l = tjfui::textBlock(text, ui::kFontBody, 46.0f, ui::kInkDim, _listRect.size.width - 200.0f,
                                TextHAlignment::CENTER);
    l->setAnchorPoint(Vec2(0.5f, 1.0f));
    l->setPosition(0.0f, 0.0f);
    _listMessage->addChild(l);
}

void ReplayListPanel::setEntries(std::vector<Entry> entries) {
    _entries = std::move(entries);
    _offset = 0.0f;
    for (Row& r : _rows) r.bound = -1;
    _selected = -1;
    if (!_entries.empty()) select(0);
    refreshRows();
    refreshDetail();
}

float ReplayListPanel::maxOffset() const { return std::max(0.0f, _entries.size() * _rowH - _listRect.size.height); }

void ReplayListPanel::setOffset(float offset) {
    _offset = std::max(0.0f, std::min(offset, maxOffset()));
    refreshRows();
}

void ReplayListPanel::onScroll(const Vec2& world, float amount) {
    if (_listRect.containsPoint(_content->convertToNodeSpace(world))) setOffset(_offset + amount * _rowH);
}

bool ReplayListPanel::onKey(EventKeyboard::KeyCode key) {
    using K = EventKeyboard::KeyCode;
    if (_entries.empty()) return false;
    switch (key) {
        case K::KEY_UP_ARROW: select(std::max(0, _selected - 1)); break;
        case K::KEY_DOWN_ARROW: select(std::min((int)_entries.size() - 1, _selected + 1)); break;
        case K::KEY_ENTER:
        case K::KEY_KP_ENTER: watchSelected(); break;
        default: return false;
    }
    // keep the selection visible
    const float top = _selected * _rowH;
    if (top < _offset) setOffset(top);
    else if (top + _rowH > _offset + _listRect.size.height) setOffset(top + _rowH - _listRect.size.height);
    return true;
}

void ReplayListPanel::onClosed() {
    HWApi::getInstance()->cancel(_request);
    _request = 0;
}

void ReplayListPanel::refreshRows() {
    const int count = (int)_entries.size();
    const int first = std::max(0, (int)std::floor(_offset / _rowH));
    const int last = std::min(count - 1, (int)std::floor((_offset + _listRect.size.height) / _rowH));
    const int pool = (int)_rows.size();
    for (Row& r : _rows) r.node->setVisible(false);
    for (int k = first; k <= last; ++k) {
        Row& r = _rows[k % pool];
        if (r.bound != k) bindRow(r, k);
        const bool sel = k == _selected;
        r.selectedBg->setVisible(sel);
        r.bg->setVisible(!sel);
        r.name->setColor(sel ? Color3B::WHITE : ui::kInk);
        r.sub->setColor(sel ? Color3B(225, 236, 255) : ui::kInkDim);
        r.rank->setColor(sel ? Color3B::WHITE : ui::kInkDim);
        r.time->setColor(sel ? Color3B::WHITE : ui::kInk);
        const float yTop = _listRect.getMaxY() - (k * _rowH - _offset);
        r.node->setPosition(_listRect.origin.x, yTop - _rowH);
        r.node->setVisible(true);
    }
}

void ReplayListPanel::bindRow(Row& r, int index) {
    const Entry& e = _entries[index];
    r.bound = index;
    const float maxW = _listRect.size.width - 170.0f - 420.0f;
    if (e.local) {
        const SavedRun s = runOf(e);
        r.rank->setString("YOU");
        // A level's replays: "You"; My Replays (no sort menu, all levels): the level's name.
        std::string name = _sort ? playerName() : (trim(s.levelName).empty() ? "(untitled)" : trim(s.levelName));
        if (!s.completed) name += " (unfinished)";
        ui::setEllipsized(r.name, name, maxW);
        std::string status = s.uploadedId ? "uploaded #" + std::to_string(s.uploadedId)
                                          : (!s.file.empty() ? "saved on this PC" : "this session, not saved");
        std::string sub = (s.date.empty() ? std::string("just now") : s.date) + "  \xC2\xB7  " + status;
        ui::setEllipsized(r.sub, sub, maxW);
        r.time->setString(s.completed ? formatTime(s.frames) : "DNF");
        r.stars->setVisible(false);
    } else {
        const ReplayInfo& i = e.online;
        r.rank->setString(std::to_string(index + 1 - (int)std::count_if(_entries.begin(), _entries.end(),
                                                                          [](const Entry& x) { return x.local; })));
        ui::setEllipsized(r.name, trim(i.userName), maxW);
        std::string sub = ui::formatDate(i.created) + "  \xC2\xB7  " + ui::characterName(i.character) + "  \xC2\xB7  " +
                          ui::formatCount(i.views) + " views";
        ui::setEllipsized(r.sub, sub, maxW);
        r.time->setString(i.completed() ? formatTime(i.frames) : "DNF");
        r.stars->setVisible(true);
        r.stars->setRating(i.averageRating());
    }
}

void ReplayListPanel::select(int index) {
    if (index < 0 || index >= (int)_entries.size()) return;
    _selected = index;
    refreshRows();
    refreshDetail();
}

SavedRun ReplayListPanel::runOf(const Entry& e) const {
    if (e.runSerial) {
        if (RunRecord* run = runBySerial(e.runSerial)) return run->toSavedRun();
    }
    return e.saved;
}

void ReplayListPanel::refreshDetail() {
    const bool has = _selected >= 0 && _selected < (int)_entries.size();
    _detail->setVisible(has);
    if (!has) return;
    const Entry& e = _entries[_selected];
    const float w = _detailRect.size.width;
    if (e.local) {
        const SavedRun s = runOf(e);
        ui::setEllipsized(_dName, "Your run" + (trim(s.levelName).empty() ? std::string() : " \xC2\xB7 " + trim(s.levelName)), w);
        _dTime->setString(s.completed ? formatTime(s.frames) : "Did not finish");
        ui::setEllipsized(_dLine1, ui::characterName(s.character) + "  \xC2\xB7  " +
                                       (s.date.empty() ? std::string("this session") : s.date), w);
        _dStars->setVisible(false);
        _dRating->setString("");
        _dLine2->setString(s.uploadedId ? "Uploaded to totaljerkface.com as replay #" + std::to_string(s.uploadedId)
                                        : (s.file.empty() ? "Recorded in OpenWheels (not saved yet)"
                                                          : "Recorded in OpenWheels, saved on this PC"));
        _dComment->setString(
            "Your inputs, recorded the way the browser game records replays (one sample per 30 Hz frame).");
        _dNote->setString(s.completed ? "" : "Unfinished runs upload as 200 s, like in the browser game.");
        _rateBtn->setVisible(false);
        _saveBtn->setVisible(true);
        _saveBtn->setEnabled(s.file.empty() && !_busy);
        _uploadBtn->setVisible(true);
        _uploadBtn->setEnabled(!s.uploadedId && !_busy && (int)s.input.keys.size() <= kMaxReplayFrames);
        _deleteBtn->setVisible(true);
        _deleteBtn->setEnabled(!s.file.empty() && !_busy);
    } else {
        const ReplayInfo& i = e.online;
        ui::setEllipsized(_dName, trim(i.userName), w);
        _dTime->setString(i.completed() ? formatTime(i.frames) : "Did not finish");
        ui::setEllipsized(_dLine1, ui::characterName(i.character) + "  \xC2\xB7  " + ui::formatDate(i.created) +
                                       "  \xC2\xB7  " + ui::formatThousands(i.views) + " views", w);
        _dStars->setVisible(true);
        _dStars->setRating(i.averageRating());
        _dRating->setString(ui::formatRating(i.averageRating()) + "  (" + ui::formatThousands(i.votes) +
                            (i.votes == 1 ? " vote)" : " votes)"));
        std::string origin = "Recorded in the browser game";
        if (!i.version.empty()) origin += " v" + i.version;
        if (i.architecture == kArchitecture) origin = "Uploaded from OpenWheels";
        if (playableCharacter(i.character) != i.character)
            origin += "  \xC2\xB7  played as " + ui::characterName(playableCharacter(i.character));
        ui::setEllipsized(_dLine2, origin, w);
        const std::string comment = trim(i.comment);
        _dComment->setString(comment.empty() ? "No comment." : comment);
        _dNote->setString(
            "Browser replays store only the keys pressed. OpenWheels re-simulates them with its own physics, so "
            "the replay is approximate and can go differently.");
        _rateBtn->setVisible(true);
        _rateBtn->setEnabled(!_busy);
        _saveBtn->setVisible(false);
        _uploadBtn->setVisible(false);
        _deleteBtn->setVisible(false);
    }
    _watchBtn->setEnabled(!_busy);
}

// ---- actions --------------------------------------------------------------------------------------

void ReplayListPanel::startWatching(const OnlineLevelInfo& level, const ReplayInfo& info, const ReplayInput& input,
                                    const std::string& flashXml, const std::string& title) {
    BrowserState& st = OnlineLevelBrowser::state();
    st.returnPending = true;
    st.parkedScene = Director::getInstance()->getRunningScene();
    std::string error;
    if (!watch(level, info, input, flashXml, title, &error)) {
        st.returnPending = false;
        st.parkedScene = nullptr;
        setStatus("", false);
        tjfui::alert("Couldn't play this replay", error);
        return;
    }
    dismiss();
}

void ReplayListPanel::watchSelected() {
    if (_busy || _selected < 0 || _selected >= (int)_entries.size()) return;
    const Entry e = _entries[_selected];
    RefPtr<ReplayListPanel> self(this);
    const int gen = ++_generation;
    if (e.local) {
        const SavedRun run = runOf(e);
        OnlineLevelInfo level = e.level;
        if (!level.id) {
            level.id = run.levelId;
            level.authorId = run.levelAuthorId;
            level.name = run.levelName;
        }
        ReplayInfo info;
        info.levelId = run.levelId;
        info.userName = playerName();
        info.character = run.character;
        info.frames = run.completed ? run.frames : kMaxReplayFrames;
        info.architecture = kArchitecture;
        setStatus("Loading level...", true);
        _request = HWApi::getInstance()->downloadLevel(
            level, false, [self, gen, level, info, run](bool ok, const std::string& error, const std::string& xml) {
                ReplayListPanel* p = self.get();
                if (p->_closing || gen != p->_generation) return;
                if (!ok) {
                    p->setStatus("", false);
                    tjfui::alert("Couldn't load the level", error);
                    return;
                }
                p->startWatching(level, info, run.input, xml, "Your run");
            });
        return;
    }
    setStatus(ReplayApi::get()->isCached(e.online.id) ? "Loading replay..." : "Downloading replay...", true);
    _request = ReplayApi::get()->download(
        e.online, e.level, [self, gen, e](bool ok, const std::string& error, const ReplayInput& input, const std::string& xml) {
            ReplayListPanel* p = self.get();
            if (p->_closing || gen != p->_generation) return;
            if (!ok) {
                p->setStatus("", false);
                tjfui::alert("Couldn't download the replay", error);
                return;
            }
            p->startWatching(e.level, e.online, input, xml, trim(e.online.userName));
        });
}

void ReplayListPanel::rateSelected() {
    if (_selected < 0 || _entries[_selected].local) return;
    const ReplayInfo info = _entries[_selected].online;
    account::ensureLoggedIn("Log in to rate replays.", [info]() {
        account::RatePanel::show("replay", info.id, trim(info.userName) + "  \xC2\xB7  " +
                                                        (info.completed() ? formatTime(info.frames) : "DNF"));
    });
}

void ReplayListPanel::saveSelected() {
    if (_selected < 0 || !_entries[_selected].local) return;
    Entry& e = _entries[_selected];
    SavedRun run = runOf(e);
    if (!run.file.empty()) return;
    std::string error;
    if (!saveRun(run, &error)) {
        tjfui::alert("Couldn't save the replay", error);
        return;
    }
    if (RunRecord* rec = e.runSerial ? runBySerial(e.runSerial) : nullptr) rec->saved = run;
    e.saved = run;
    for (Row& r : _rows) r.bound = -1;
    refreshRows();
    refreshDetail();
    ui::showToast("Replay saved", {"Kept on this PC (My Replays)."}, 0.0f);
}

void ReplayListPanel::uploadSelected() {
    if (_selected < 0 || !_entries[_selected].local) return;
    const int index = _selected;
    RefPtr<ReplayListPanel> self(this);
    account::ensureLoggedIn("Log in to upload your replay to totaljerkface.com.", [self, index]() {
        ReplayListPanel* p = self.get();
        if (p->_closing || index >= (int)p->_entries.size()) return;
        const SavedRun run = p->runOf(p->_entries[index]);
        UploadReplayPanel::show(run, [self, index](int newId) {
            ReplayListPanel* q = self.get();
            if (index >= (int)q->_entries.size()) return;
            Entry& e = q->_entries[index];
            SavedRun saved = q->runOf(e);
            saved.uploadedId = newId;
            std::string error;
            saveRun(saved, &error);   // keep the upload id with the run
            if (RunRecord* rec = e.runSerial ? runBySerial(e.runSerial) : nullptr) rec->saved = saved;
            e.saved = saved;
            if (!q->_closing) {
                for (Row& r : q->_rows) r.bound = -1;
                q->refreshRows();
                q->refreshDetail();
            }
        });
    });
}

void ReplayListPanel::deleteSelected() {
    if (_selected < 0 || !_entries[_selected].local) return;
    const SavedRun run = runOf(_entries[_selected]);
    if (run.file.empty()) return;
    RefPtr<ReplayListPanel> self(this);
    tjfui::confirm("Delete replay?", "Delete this saved run from this PC? Uploaded replays stay on the site.",
                   "DELETE", "KEEP", [self, run](bool yes) {
                       if (!yes) return;
                       deleteRun(run);
                       for (RunRecord* r : recentRuns(0))
                           if (r->saved.file == run.file) r->saved = SavedRun();
                       if (!self->_closing) self->reload();
                   });
}

// ---- ReplayPanel --------------------------------------------------------------------------------------

ReplayPanel* ReplayPanel::show(const OnlineLevelInfo& level) {
    auto* p = new (std::nothrow) ReplayPanel();
    if (p && p->init(level)) {
        p->autorelease();
        p->present();
        return p;
    }
    delete p;
    return nullptr;
}

void ReplayPanel::rateReplay(int replayId, int rating, account::ReplyCallback done) {
    ReplayApi::get()->rate(replayId, rating, std::move(done));
}

bool ReplayPanel::init(const OnlineLevelInfo& level) {
    _level = level;
    if (!initList("Replays", trim(level.name).empty() ? "(untitled)" : trim(level.name), true)) return false;
    reload();
    return true;
}

void ReplayPanel::reload() {
    HWApi::getInstance()->cancel(_request);
    const int gen = ++_generation;
    std::vector<Entry> local;
    std::vector<std::string> files;
    for (RunRecord* run : recentRuns(_level.id)) {
        Entry e;
        e.local = true;
        e.runSerial = run->serial;
        e.level = _level;
        e.saved = run->saved;
        if (!run->saved.file.empty()) files.push_back(run->saved.file);
        local.push_back(e);
    }
    for (const SavedRun& s : listSavedRuns(_level.id)) {
        if (std::find(files.begin(), files.end(), s.file) != files.end()) continue;
        Entry e;
        e.local = true;
        e.saved = s;
        e.level = _level;
        local.push_back(e);
    }
    setEntries(local);
    setListMessage("Loading replays...", true);
    _count->setString("");
    setStatus("", false);
    RefPtr<ReplayPanel> self(this);
    _request = ReplayApi::get()->listByLevel(
        _level.id, kSorts[s_sortIndex], 1,
        [self, gen, local](bool ok, const std::string& error, const std::vector<ReplayInfo>& list, int, int perPage) {
            ReplayPanel* p = self.get();
            if (p->_closing || gen != p->_generation) return;
            p->_request = 0;
            if (!ok) {
                p->setListMessage(local.empty() ? "Couldn't load replays.\n" + error : "", false);
                p->_count->setString("couldn't load the site's replays");
                return;
            }
            noteRecord(p->_level.id, list, kSorts[s_sortIndex] == ReplaySort::Fastest);
            std::vector<Entry> all = local;
            for (const ReplayInfo& r : list) {
                Entry e;
                e.online = r;
                e.level = p->_level;
                all.push_back(e);
            }
            const int selected = p->_selected;
            p->setEntries(all);
            if (selected > 0 && selected < (int)all.size()) p->select(selected);
            const int n = (int)list.size();
            std::string count = ui::formatThousands(n) + (n == 1 ? " replay" : " replays");
            if (perPage > 0 && n >= perPage) count = ui::formatThousands(n) + "+ replays";
            p->_count->setString(count);
            p->setListMessage(all.empty() ? "No replays yet.\nPlay the level and upload yours!" : "", false);
        });
}

// ---- MyReplaysPanel -----------------------------------------------------------------------------------

MyReplaysPanel* MyReplaysPanel::show() {
    auto* p = new (std::nothrow) MyReplaysPanel();
    if (p && p->init()) {
        p->autorelease();
        p->present();
        return p;
    }
    delete p;
    return nullptr;
}

bool MyReplaysPanel::init() {
    if (!initList("My Replays", "Your runs of online levels, kept on this PC", false)) return false;
    reload();
    return true;
}

void MyReplaysPanel::reload() {
    std::vector<Entry> entries;
    std::vector<std::string> files;
    for (RunRecord* run : recentRuns(0)) {
        Entry e;
        e.local = true;
        e.runSerial = run->serial;
        e.level = run->level;
        e.saved = run->saved;
        if (!run->saved.file.empty()) files.push_back(run->saved.file);
        entries.push_back(e);
    }
    for (const SavedRun& s : listSavedRuns(0)) {
        if (std::find(files.begin(), files.end(), s.file) != files.end()) continue;
        Entry e;
        e.local = true;
        e.saved = s;
        e.level.id = s.levelId;
        e.level.authorId = s.levelAuthorId;
        e.level.name = s.levelName;
        entries.push_back(e);
    }
    setEntries(entries);
    const int n = (int)entries.size();
    _count->setString(ui::formatThousands(n) + (n == 1 ? " run" : " runs"));
    setListMessage(entries.empty() ? "No runs yet.\nPlay an online level; your runs show up here." : "", false);
    setStatus("", false);
}

// ---- UploadReplayPanel --------------------------------------------------------------------------------

UploadReplayPanel* UploadReplayPanel::show(const SavedRun& run, std::function<void(int)> uploaded) {
    auto* p = new (std::nothrow) UploadReplayPanel();
    if (p && p->init(run, std::move(uploaded))) {
        p->autorelease();
        p->present();
        return p;
    }
    delete p;
    return nullptr;
}

bool UploadReplayPanel::init(const SavedRun& run, std::function<void(int)> uploaded) {
    if (!initPanel(Size(2000.0f, 1500.0f), "Upload Replay")) return false;
    _run = run;
    _uploaded = std::move(uploaded);
    const float L = 130.0f, W = _size.width - 260.0f;
    float y = _top;
    const std::string who = account::TjfAccount::get()->displayName();
    Label* what = tjfui::textBlock(
        "\"" + trim(run.levelName) + "\"  \xC2\xB7  " + (run.completed ? formatTime(run.frames) : std::string("unfinished")) +
            "  \xC2\xB7  " + ui::characterName(run.character) + "\nUploaded as " + who +
            ". Everyone will be able to watch it on totaljerkface.com.",
        ui::kFontBody, 46.0f, ui::kInk, W);
    what->setPosition(L, y);
    _content->addChild(what);
    y -= what->getContentSize().height + 60.0f;

    Label* cap = tjfui::label("COMMENT (OPTIONAL, 200 CHARACTERS)", ui::kFontBodyBold, 36.0f, ui::kInkDim);
    cap->setPosition(L, y);
    _content->addChild(cap);
    y -= 90.0f;
    _comment = tjfui::TextInput::create(Size(W, 130.0f), "Say something about your run", false, 200);
    // SaveReplayMenu commentsText.restrict
    _comment->setAllowed("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !@#$%^&*()_+-=;'|?/,.<>\"");
    _comment->setPosition(L + W * 0.5f, y);
    _content->addChild(_comment);
    y -= 120.0f;

    Label* honest = tjfui::textBlock(
        "Heads up: OpenWheels' physics are not the browser game's. Browser players will see this replay marked as "
        "not 100% accurate, and it may play out differently for them.",
        ui::kFontBody, 40.0f, ui::kInkDim, W);
    honest->setPosition(L, y);
    _content->addChild(honest);

    _message = tjfui::textBlock("", ui::kFontBodyBold, 42.0f, kError, W);
    _message->setPosition(L, 330.0f);
    _content->addChild(_message);

    _uploadBtn = ui::Button::create("UPLOAD REPLAY", Size(760.0f, 150.0f), ui::Button::window("blue"), 56.0f);
    _uploadBtn->setIcon(tjfui::icon("upload", 64.0f));
    _uploadBtn->setPosition(_size.width * 0.5f, 150.0f);
    _uploadBtn->setCallback([this]() { submit(); });
    _content->addChild(_uploadBtn);
    _firstField = _comment;
    return true;
}

bool UploadReplayPanel::onKey(EventKeyboard::KeyCode key) {
    using K = EventKeyboard::KeyCode;
    if (key == K::KEY_V && _ctrlDown) {
        _comment->paste();
        return true;
    }
    return false;
}

void UploadReplayPanel::submit() {
    if (_busy) return;
    if ((int)_run.input.keys.size() > kMaxReplayFrames) {
        _message->setString("Sorry, your replay must be less than 200 seconds.");
        return;
    }
    // An explicit second confirmation: this publishes the run under the player's name.
    RefPtr<UploadReplayPanel> self(this);
    tjfui::confirm("Upload replay?",
                   "Upload this run to totaljerkface.com as " + account::TjfAccount::get()->displayName() +
                       "? Everyone will be able to watch it.",
                   "UPLOAD", "CANCEL", [self](bool yes) {
                       UploadReplayPanel* p = self.get();
                       if (!yes || p->_closing) return;
                       p->_busy = true;
                       p->_uploadBtn->setEnabled(false);
                       p->_message->setString("Uploading...");
                       p->_message->setColor(ui::kInkDim);
                       ReplayApi::get()->upload(p->_run, trim(p->_comment->text()), [self](const account::Reply& reply) {
                           UploadReplayPanel* q = self.get();
                           q->_busy = false;
                           if (!reply.ok) {
                               if (q->_closing) return;
                               q->_message->setString(reply.message);
                               q->_message->setColor(kError);
                               q->_uploadBtn->setEnabled(true);
                               return;
                           }
                           const int id = std::atoi(reply.value.c_str());
                           if (q->_uploaded) q->_uploaded(id);
                           if (!q->_closing) q->dismiss();
                           ui::showToast("Replay uploaded", {"Replay #" + std::to_string(id) + " is on totaljerkface.com."}, 0.0f);
                       });
                   });
}

}  // namespace replays
}  // namespace online
