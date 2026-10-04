// NET (PC addition): ghost race session (lobby, level start, countdown, snapshots, results), see
// RaceSession.h and docs/RACE.md.
#include "net/race/RaceSession.h"

#include <algorithm>
#include <cmath>

#include "CharacterB2D.h"
#include "FinishLine.h"
#include "Gameplay.h"
#include "HWWindow.h"
#include "HWWindowDelegate.h"
#include "LevelB2D.h"
#include "LevelSession.h"
#include "LevelStore.h"
#include "LevelUIHelpers.h"
#include "MainMenu.h"
#include "Session.h"
#include "Settings.h"
#include "net/LevelTransfer.h"
#include "net/NetLoop.h"
#include "net/NetPlatform.h"
#include "net/NetUi.h"
#include "net/race/RaceUi.h"
#include "online/FlashLevelConverter.h"
#include "online/OnlineUi.h"

USING_NS_CC;

namespace race {

namespace {

constexpr int kInviteAlertTag = 7201;
constexpr double kSnapshotInterval = 1.0 / 25.0;
constexpr double kCountdownSeconds = 3.0;
constexpr double kInviteTimeout = 70.0;
constexpr double kLoadTimeout = 30.0;
constexpr const char kSep = '\x1f';

double now() { return net::NetLoop::now(); }

std::string clean(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\n' || c == '\r' || c == kSep) c = ' ';
        out += c;
    }
    return out;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t p = s.find(sep, start);
        out.push_back(s.substr(start, p == std::string::npos ? std::string::npos : p - start));
        if (p == std::string::npos) break;
        start = p + 1;
    }
    return out;
}

const char* phaseName(Phase p) {
    switch (p) {
    case Phase::Idle: return "idle";
    case Phase::Lobby: return "lobby";
    case Phase::Loading: return "loading";
    case Phase::Countdown: return "countdown";
    case Phase::Racing: return "racing";
    case Phase::Results: return "results";
    }
    return "idle";
}

Phase phaseFromName(const std::string& s) {
    if (s == "lobby") return Phase::Lobby;
    if (s == "loading") return Phase::Loading;
    if (s == "countdown") return Phase::Countdown;
    if (s == "racing") return Phase::Racing;
    if (s == "results") return Phase::Results;
    return Phase::Idle;
}

void writeLevel(net::Message& m, const RaceLevel& l, bool withXml) {
    m.set("kind", l.kind);
    m.set("name", l.name);
    m.set("chapter", l.chapter);
    m.set("level", l.level);
    m.set("forced", l.forced);
    m.set("fchar", l.forcedCharacter);
    if (withXml) m.set("xml", l.xml);
}

RaceLevel readLevel(const net::Message& m) {
    RaceLevel l;
    l.kind = m.get("kind");
    l.name = m.get("name");
    l.chapter = static_cast<int>(m.getInt("chapter"));
    l.level = static_cast<int>(m.getInt("level"));
    l.forced = m.getBool("forced");
    l.forcedCharacter = static_cast<int>(m.getInt("fchar", 1));
    l.xml = m.get("xml");
    return l;
}

Gameplay* findGameplay(Scene* scene) {
    if (!scene) return nullptr;
    for (Node* child : scene->getChildren()) {
        if (Gameplay* g = dynamic_cast<Gameplay*>(child)) return g;
    }
    return nullptr;
}

bool sceneHasMainMenu(Scene* scene) {
    if (!scene) return false;
    for (Node* child : scene->getChildren()) {
        if (dynamic_cast<MainMenu*>(child)) return true;
    }
    return false;
}

// FinishLine keeps its art container protected; the finish position is that container's.
struct FinishLineAccess : FinishLine {
    static Node* art(FinishLine* f) { return f->*(&FinishLineAccess::_mc); }
};

class InviteDelegate : public HWWindowDelegate {
public:
    static InviteDelegate* get() {
        static InviteDelegate d;
        return &d;
    }
    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override {
        if (window->getTag() != kInviteAlertTag) return;
        _answer = levelui::buttonIndex(buttonTag, window) == 1 ? 1 : 0;
    }
    void hwWindowWasDismissed(HWWindow* window) override {
        if (window->getTag() != kInviteAlertTag) return;
        const int answer = _answer;
        _answer = 0;
        if (_onAnswer) _onAnswer(window, answer == 1);
    }
    std::function<void(HWWindow*, bool)> _onAnswer;
    int _answer = 0;
};

}  // namespace

// ---- small helpers -------------------------------------------------------------------------------

const char* statusName(Status s) {
    switch (s) {
    case Status::Lobby: return "lobby";
    case Status::Loading: return "loading";
    case Status::Loaded: return "loaded";
    case Status::Racing: return "racing";
    case Status::Ejected: return "ejected";
    case Status::Dead: return "dead";
    case Status::Finished: return "finished";
    case Status::GaveUp: return "gaveup";
    case Status::Left: return "left";
    }
    return "lobby";
}

Status statusFromName(const std::string& s) {
    if (s == "loading") return Status::Loading;
    if (s == "loaded") return Status::Loaded;
    if (s == "racing") return Status::Racing;
    if (s == "ejected") return Status::Ejected;
    if (s == "dead") return Status::Dead;
    if (s == "finished") return Status::Finished;
    if (s == "gaveup") return Status::GaveUp;
    if (s == "left") return Status::Left;
    return Status::Lobby;
}

bool statusIsFinal(Status s) {
    return s == Status::Finished || s == Status::GaveUp || s == Status::Left;
}

std::string RaceLevel::kindLabel() const {
    if (kind == "campaign") return "Campaign";
    if (kind == "online") return "Online level";
    return "Your level";
}

Color3B RaceSession::slotColor(int slot) {
    static const Color3B colors[] = {Color3B(255, 120, 95), Color3B(110, 175, 255), Color3B(120, 230, 120),
                                     Color3B(255, 215, 80)};
    return colors[((slot % 4) + 4) % 4];
}

std::vector<int> RaceSession::characterIds() {
    std::vector<int> ids;
    for (const Value& v : Settings::getInstance()->getAllCharactersData()) {
        if (v.getType() != Value::Type::MAP) continue;
        const ValueMap& m = v.asValueMap();
        auto it = m.find("id");
        if (it != m.end()) ids.push_back(it->second.asInt());
    }
    return ids;
}

std::string RaceSession::characterName(int characterId) {
    return online::ui::characterName(characterId);
}

// ---- singleton -----------------------------------------------------------------------------------

RaceSession* RaceSession::get() {
    static RaceSession* s = new RaceSession();
    return s;
}

RaceSession::RaceSession() = default;

void RaceSession::ensureTicking() {
    if (_ticking) return;
    _ticking = true;
    Director* d = Director::getInstance();
    d->getScheduler()->schedule([this](float dt) { tick(dt); }, this, 0.0f, false, "ow_race_tick");
    EventDispatcher* e = d->getEventDispatcher();
    e->addCustomEventListener("levelCompleted", [this](EventCustom*) { onLevelComplete(); });
    e->addCustomEventListener("characterDead", [this](EventCustom*) { onCharacterEvent("dead"); });
    e->addCustomEventListener("characterEjected", [this](EventCustom*) { onCharacterEvent("ejected"); });
    _statsSince = now();
}

void RaceSession::setPhase(Phase p) {
    if (_phase == p) return;
    _phase = p;
    if (p == Phase::Results) _resultsShown = false;
    changed();
}

void RaceSession::changed() {
    if (LobbyPanel* lp = dynamic_cast<LobbyPanel*>(_lobbyPanel.get())) {
        if (lp->getParent()) lp->refresh();
    }
    if (ResultsPanel* rp = dynamic_cast<ResultsPanel*>(_resultsPanel.get())) {
        if (rp->getParent()) rp->refresh();
    }
}

const RacePlayer* RaceSession::player(int id) const {
    for (const RacePlayer& p : _players) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

RacePlayer* RaceSession::mutablePlayer(int id) {
    for (RacePlayer& p : _players) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

int RaceSession::freeSlot() const {
    for (int s = 0; s < kMaxPlayers; ++s) {
        bool used = false;
        for (const RacePlayer& p : _players) {
            if (p.slot == s && p.status != Status::Left) used = true;
        }
        if (!used) return s;
    }
    return static_cast<int>(_players.size()) % kMaxPlayers;
}

double RaceSession::raceClock() const {
    if (_phase != Phase::Racing && _phase != Phase::Results) return 0.0;
    return std::max(0.0, now() - _goAt);
}

double RaceSession::countdownLeft() const {
    return _phase == Phase::Countdown ? std::max(0.0, _goAt - now()) : 0.0;
}

float RaceSession::progressOf(int playerId) const {
    const RacePlayer* p = player(playerId);
    if (p && p->status == Status::Finished) return 1.0f;
    if (!_haveFinish || !_haveStart) return -1.0f;
    Vec2 focus;
    if (playerId == _localId) {
        focus = _localFocus;
    } else {
        auto it = _tracks.find(playerId);
        if (it == _tracks.end() || !it->second.hasFrames()) return -1.0f;
        focus = it->second.latestFocus();
    }
    // how far along the start -> finish line the rider is
    const Vec2 line = _finishPos - _startFocus;
    const float len2 = line.lengthSquared();
    if (len2 < 1.0f) return -1.0f;
    return std::max(0.0f, std::min(0.99f, (focus - _startFocus).dot(line) / len2));
}

std::string RaceSession::statusLine() const {
    if (_phase == Phase::Lobby) {
        std::string why;
        if (_host) return canStart(&why) ? std::string("Everyone is ready.") : why;
        const RacePlayer* me = localPlayer();
        return (me && me->ready) ? "Waiting for " + _hostName + " to start the race\xE2\x80\xA6"
                                 : std::string("Pick your rider, then press Ready.");
    }
    if (_phase == Phase::Loading) return "Loading the level\xE2\x80\xA6";
    return std::string();
}

bool RaceSession::suppressVictoryMenu() const {
    return _phase == Phase::Racing || _phase == Phase::Results || _phase == Phase::Countdown;
}

// ---- messages ------------------------------------------------------------------------------------

net::Message RaceSession::helloMessage() const {
    net::Message m("hello");
    m.set("app", "OpenWheels");
    m.set("proto", kRaceProtocol);
    m.set("purpose", "race");
    m.set("name", net::LevelTransfer::getInstance()->playerName());
    m.set("dev", net::deviceName());
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    m.set("plat", "phone");
#else
    m.set("plat", "pc");
#endif
    m.set("id", net::LanDiscovery::getInstance()->instanceId());
    return m;
}

net::Message RaceSession::lobbyMessage() const {
    net::Message m("lobby");
    m.set("phase", phaseName(_phase));
    writeLevel(m, _level, false);
    m.set("host", _hostName);
    std::string lines;
    for (const RacePlayer& p : _players) {
        if (!lines.empty()) lines += '\n';
        lines += std::to_string(p.id) + kSep + clean(p.name) + kSep + p.platform + kSep + std::to_string(p.slot) + kSep +
                 std::to_string(p.character) + kSep + (p.ready ? "1" : "0") + kSep + statusName(p.status) + kSep +
                 std::to_string(p.timeMs) + kSep + std::to_string(p.place);
    }
    m.set("players", lines);
    return m;
}

void RaceSession::readLobby(const net::Message& m) {
    const RaceLevel incoming = readLevel(m);
    // the lobby message carries no XML: keep the one from "load"
    const std::string xml = _level.xml;
    _level = incoming;
    if (_level.xml.empty()) _level.xml = xml;
    _hostName = m.get("host");
    std::vector<RacePlayer> players;
    for (const std::string& line : split(m.get("players"), '\n')) {
        if (line.empty()) continue;
        const std::vector<std::string> f = split(line, kSep);
        if (f.size() < 9) continue;
        RacePlayer p;
        p.id = std::atoi(f[0].c_str());
        p.name = f[1];
        p.platform = f[2];
        p.slot = std::atoi(f[3].c_str());
        p.character = std::atoi(f[4].c_str());
        p.ready = f[5] == "1";
        p.status = statusFromName(f[6]);
        p.timeMs = std::atoi(f[7].c_str());
        p.place = std::atoi(f[8].c_str());
        players.push_back(p);
    }
    _players = players;
}

void RaceSession::broadcast(const net::Message& m, int exceptId) {
    for (auto& kv : _conns) {
        if (kv.first == exceptId || !kv.second.channel) continue;
        kv.second.channel->send(m);
    }
}

void RaceSession::sendToHost(const net::Message& m) {
    if (_hostChannel) _hostChannel->send(m);
}

void RaceSession::broadcastLobby() {
    if (!_host) return;
    broadcast(lobbyMessage());
    changed();
}

// ---- entry points --------------------------------------------------------------------------------

void openRaceMenu() { RaceSession::get()->openMenu(); }

bool suppressVictoryMenu() { return RaceSession::get()->suppressVictoryMenu(); }

void RaceSession::openMenu() {
    ensureTicking();
    if (_phase == Phase::Idle) {
        RaceLevel l;
        l.kind = "campaign";
        Settings* s = Settings::getInstance();
        l.chapter = std::max(0, s->getSelectedChapter());
        l.level = std::max(0, s->getSelectedLevel());
        ValueMap data = s->getLevelData(l.chapter, l.level);
        if (data.empty()) {
            l.chapter = 0;
            l.level = 0;
        }
        hostLevel(l);   // fills in name / forced character
        return;
    }
    if (_phase == Phase::Lobby) {
        if (!_lobbyPanel || !_lobbyPanel->getParent()) {
            LobbyPanel* p = LobbyPanel::create();
            _lobbyPanel = p;
            p->present();
        }
    }
}

void RaceSession::setLevel(const RaceLevel& input) {
    if (!_host || (_phase != Phase::Lobby && _phase != Phase::Idle)) return;
    RaceLevel l = input;
    if (l.kind == "campaign") {
        Settings* s = Settings::getInstance();
        ValueMap data = s->getLevelData(l.chapter, l.level);
        auto name = data.find("name");
        if (name != data.end()) l.name = name->second.asString();
        // levelData.plist: the chapter's "characterIndex" is a character id (-1 = any); a level may
        // say "force_character" itself.
        ValueMap chapter = s->getChapterData(l.chapter);
        const int forcedId = chapter.count("characterIndex") ? chapter["characterIndex"].asInt() : -1;
        l.forced = forcedId > 0;
        auto lf = data.find("force_character");
        if (lf != data.end()) l.forced = lf->second.asBool() && forcedId > 0;
        if (l.forced) l.forcedCharacter = forcedId;
        if (l.name.empty()) l.name = "Level " + std::to_string(l.level + 1);
    }
    _level = l;
    for (RacePlayer& p : _players) {
        if (_level.forced) p.character = _level.forcedCharacter;
        if (p.id != _localId) p.ready = false;   // a new level: everyone confirms again
    }
    broadcastLobby();
}

void RaceSession::hostLevel(const RaceLevel& level) {
    ensureTicking();
    if (_phase != Phase::Idle && !_host) {
        levelui::showAlert(0, "Already in a Race", "Leave the race you joined first.", "OK", "", nullptr);
        return;
    }
    if (_phase != Phase::Idle && _phase != Phase::Lobby) {
        levelui::showAlert(0, "Race in Progress", "Finish or close the current race first.", "OK", "", nullptr);
        return;
    }
    net::LevelTransfer::getInstance()->start();
    if (_phase == Phase::Idle) {
        reset();
        _host = true;
        _localId = 1;
        _nextPlayerId = 2;
        _hostName = net::LevelTransfer::getInstance()->playerName();
        RacePlayer me;
        me.id = 1;
        me.name = _hostName;
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
        me.platform = "phone";
#else
        me.platform = "pc";
#endif
        me.slot = 0;
        me.character = Settings::getInstance()->getSelectedCharacterId();
        if (_autoCharacter > 0) me.character = _autoCharacter;
        me.ready = true;
        _players.push_back(me);
        _phase = Phase::Lobby;
    }
    setLevel(level);
    if (_level.forced) {
        if (RacePlayer* me = mutablePlayer(_localId)) me->character = _level.forcedCharacter;
    }
    log("race: hosting \"%s\" (%s)", _level.name.c_str(), _level.kind.c_str());
    if (!_lobbyPanel || !_lobbyPanel->getParent()) {
        LobbyPanel* p = LobbyPanel::create();
        _lobbyPanel = p;
        p->present();
    } else {
        changed();
    }
}

void RaceSession::setAutoTest(const std::string& spec, int players, int character) {
    _autoSpec = spec;
    _autoPlayers = std::max(2, std::min(kMaxPlayers, players));
    _autoCharacter = character;
    ensureTicking();
    net::LevelTransfer::getInstance()->start();
    log("race: test mode %s (players %d, character %d)", spec.c_str(), _autoPlayers, character);
}

// ---- host: invites -------------------------------------------------------------------------------

void RaceSession::invite(const net::Peer& peer) {
    if (_pending.count(peer.id)) return;
    startInvite(peer.id, peer.address, peer.port, peer.name);
}

void RaceSession::inviteTarget(uint32_t address, uint16_t port, const std::string& name) {
    startInvite(net::ipToString(address) + ":" + std::to_string(port), address, port, name);
}

void RaceSession::startInvite(const std::string& key, uint32_t address, uint16_t port, const std::string& name) {
    if (!_host || _phase != Phase::Lobby) return;
    int active = static_cast<int>(_players.size());
    for (auto& kv : _pending) {
        if (!kv.second.answered) ++active;
    }
    if (active >= kMaxPlayers) {
        levelui::showAlert(0, "Race Full", "A race has at most four riders.", "OK", "", nullptr);
        return;
    }
    PendingInvite inv;
    inv.peerId = key;
    inv.name = name.empty() ? net::ipToString(address) : name;
    inv.deadline = now() + kInviteTimeout;
    inv.channel = net::Channel::connect(address, port);
    inv.channel->setMaxPayload(256 * 1024);
    _pending[key] = inv;
    _invites[key] = InviteInfo{InviteInfo::Connecting, "Inviting\xE2\x80\xA6"};
    net::Channel::Handler h;
    h.onOpen = [this, key]() {
        auto p = _pending.find(key);
        if (p == _pending.end()) return;
        p->second.channel->send(helloMessage());
        net::Message m("invite");
        m.set("level", _level.name);
        m.set("kind", _level.kind);
        m.set("host", _hostName);
        m.set("players", static_cast<int>(_players.size()));
        p->second.channel->send(m);
        _invites[key] = InviteInfo{InviteInfo::Waiting, "Waiting\xE2\x80\xA6"};
        changed();
    };
    h.onMessage = [this, key](const net::Message& m) { inviteMessage(key, m); };
    h.onClosed = [this, key](const std::string&) { inviteClosed(key); };
    inv.channel->setHandler(h);
    log("race: inviting %s", inv.name.c_str());
    changed();
}
void RaceSession::inviteMessage(const std::string& peerId, const net::Message& m) {
    auto it = _pending.find(peerId);
    if (it == _pending.end()) return;
    PendingInvite& inv = it->second;
    if (m.type == "hello") {
        if (m.get("app") != "OpenWheels" || m.getInt("proto", -1) != kRaceProtocol) {
            inv.channel->abort();
            _invites[peerId] = InviteInfo{InviteInfo::Failed, "Different version"};
            _pending.erase(it);
            changed();
            return;
        }
        const std::string name = net::LevelTransfer::sanitizeName(m.get("name"));
        if (!name.empty()) inv.name = name;
        return;
    }
    if (m.type == "answer") {
        inv.answered = true;
        if (!m.getBool("accept")) {
            const std::string reason = m.get("reason");
            _invites[peerId] = InviteInfo{InviteInfo::Declined, reason == "busy" ? "Busy" : "Declined"};
            inv.channel->close();
            _pending.erase(it);
            changed();
            return;
        }
        if (_phase != Phase::Lobby || static_cast<int>(_players.size()) >= kMaxPlayers) {
            inv.channel->send(net::Message("end").set("reason", "full"));
            inv.channel->close();
            _pending.erase(it);
            _invites[peerId] = InviteInfo{InviteInfo::Failed, "Race full"};
            changed();
            return;
        }
        std::shared_ptr<net::Channel> channel = inv.channel;
        const std::string name = inv.name;
        _pending.erase(it);
        _invites[peerId] = InviteInfo{InviteInfo::Joined, "Joined"};
        hostAddPlayer(channel, name, m.get("plat"));
        return;
    }
    if (m.type == "error") {
        inv.channel->abort();
        _invites[peerId] = InviteInfo{InviteInfo::Failed, "Different version"};
        _pending.erase(it);
        changed();
    }
}

void RaceSession::inviteClosed(const std::string& peerId) {
    auto it = _pending.find(peerId);
    if (it == _pending.end()) return;
    _invites[peerId] = InviteInfo{InviteInfo::Failed, "Not reachable"};
    _pending.erase(it);
    changed();
}

void RaceSession::hostAddPlayer(const std::shared_ptr<net::Channel>& channel, const std::string& name,
                                const std::string& platform) {
    RacePlayer p;
    p.id = _nextPlayerId++;
    p.name = name;
    p.platform = platform.empty() ? "pc" : platform;
    p.slot = freeSlot();
    p.character = _level.forced ? _level.forcedCharacter : 2;
    _players.push_back(p);
    const int id = p.id;
    Conn c;
    c.playerId = id;
    c.channel = channel;
    channel->setMaxPayload(256 * 1024);
    _conns[id] = c;
    net::Channel::Handler h;
    h.onMessage = [this, id](const net::Message& m) { hostMessage(id, m); };
    h.onClosed = [this, id](const std::string& error) { hostClosed(id, error); };
    channel->setHandler(h);
    channel->send(net::Message("welcome").set("you", id));
    log("race: %s joined as player %d", name.c_str(), id);
    broadcastLobby();
}

// ---- host: messages from guests ------------------------------------------------------------------

void RaceSession::hostMessage(int playerId, const net::Message& m) {
    RacePlayer* p = mutablePlayer(playerId);
    if (!p) return;
    if (m.type == "s") {
        const std::string& d = m.get("d");
        receiveSnapshot(playerId, d);
        net::Message relay("s");
        relay.set("d", d);
        relay.set("f", playerId);
        broadcast(relay, playerId);
        return;
    }
    if (m.type == "pick") {
        if (_phase == Phase::Lobby && !_level.forced) {
            const int c = static_cast<int>(m.getInt("char", 2));
            p->character = c;
            broadcastLobby();
        }
        return;
    }
    if (m.type == "ready") {
        if (_phase == Phase::Lobby) {
            p->ready = m.getBool("ready");
            broadcastLobby();
        }
        return;
    }
    if (m.type == "loaded") {
        _loaded[playerId] = true;
        if (p->status == Status::Loading) p->status = Status::Loaded;
        broadcastLobby();
        return;
    }
    if (m.type == "st") {
        hostSetStatus(playerId, statusFromName(m.get("status")), static_cast<int>(m.getInt("time", -1)));
        return;
    }
    if (m.type == "bye") {
        _conns[playerId].channel->close();
        hostClosed(playerId, "");
        return;
    }
}

void RaceSession::hostClosed(int playerId, const std::string& error) {
    auto it = _conns.find(playerId);
    if (it == _conns.end()) return;
    it->second.channel->clearHandler();
    std::shared_ptr<net::Channel> keep = it->second.channel;
    _conns.erase(it);
    net::NetLoop::instance()->post([keep]() {});
    const RacePlayer* p = player(playerId);
    const std::string name = p ? p->name : std::string("A player");
    if (!error.empty()) log("race: %s disconnected (%s)", name.c_str(), error.c_str());
    broadcast(net::Message("left").set("id", playerId).set("name", name));
    playerLeft(playerId, name);
    broadcastLobby();
    checkResults();
}

void RaceSession::playerLeft(int id, const std::string& name) {
    if (_phase == Phase::Lobby || _phase == Phase::Idle) {
        _players.erase(std::remove_if(_players.begin(), _players.end(), [id](const RacePlayer& p) { return p.id == id; }),
                       _players.end());
    } else if (RacePlayer* p = mutablePlayer(id)) {
        p->status = Status::Left;
        _fadeOut[id] = now();
    }
    online::ui::showToast(name + " left the race", {}, 0.0f);
    changed();
}

void RaceSession::hostSetStatus(int playerId, Status s, int timeMs) {
    RacePlayer* p = mutablePlayer(playerId);
    if (!p) return;
    if (statusIsFinal(p->status) && s != Status::Left) return;   // finished / gave up stays
    p->status = s;
    if (s == Status::Finished) p->timeMs = timeMs;
    broadcastLobby();
    checkResults();
}

void RaceSession::computePlaces() {
    std::vector<RacePlayer*> finished;
    for (RacePlayer& p : _players) {
        p.place = 0;
        if (p.status == Status::Finished && p.timeMs >= 0) finished.push_back(&p);
    }
    std::sort(finished.begin(), finished.end(), [](RacePlayer* a, RacePlayer* b) { return a->timeMs < b->timeMs; });
    for (size_t i = 0; i < finished.size(); ++i) finished[i]->place = static_cast<int>(i) + 1;
}

void RaceSession::checkResults() {
    if (!_host || _phase != Phase::Racing) return;
    for (const RacePlayer& p : _players) {
        if (!statusIsFinal(p.status)) return;
    }
    computePlaces();
    setPhase(Phase::Results);
    broadcast(lobbyMessage());
    broadcast(net::Message("results"));
    log("race: results");
}

// ---- host: race control --------------------------------------------------------------------------

bool RaceSession::canStart(std::string* why) const {
    if (!_host || _phase != Phase::Lobby) return false;
    if (!_level.valid()) {
        if (why) *why = "Pick a level first.";
        return false;
    }
    if (_players.size() < 2) {
        if (why) *why = "Invite at least one player.";
        return false;
    }
    for (const RacePlayer& p : _players) {
        if (!p.ready) {
            if (why) *why = "Waiting for " + p.name + " to get ready\xE2\x80\xA6";
            return false;
        }
    }
    return true;
}

void RaceSession::start() {
    if (!canStart(nullptr)) return;
    ++_raceSerial;
    _loaded.clear();
    for (RacePlayer& p : _players) {
        p.status = Status::Loading;
        p.timeMs = -1;
        p.place = 0;
        if (_level.forced) p.character = _level.forcedCharacter;
    }
    setPhase(Phase::Loading);
    _loadStarted = now();
    broadcast(lobbyMessage());
    net::Message load("load");
    writeLevel(load, _level, true);
    load.set("race", _raceSerial);
    broadcast(load);
    log("race: starting \"%s\" with %d players", _level.name.c_str(), static_cast<int>(_players.size()));
    startLocalLevel();
}

void RaceSession::rematch() {
    if (!_host || _phase != Phase::Results) return;
    for (RacePlayer& p : _players) {
        if (p.status != Status::Left) p.ready = true;
    }
    _players.erase(std::remove_if(_players.begin(), _players.end(), [](const RacePlayer& p) { return p.status == Status::Left; }),
                   _players.end());
    _phase = Phase::Lobby;
    if (!canStart(nullptr)) {
        backToLobby();
        return;
    }
    start();
}

void RaceSession::backToLobby() {
    if (!_host) return;
    _players.erase(std::remove_if(_players.begin(), _players.end(), [](const RacePlayer& p) { return p.status == Status::Left; }),
                   _players.end());
    for (RacePlayer& p : _players) {
        p.ready = p.id == _localId;
        p.status = Status::Lobby;
        p.timeMs = -1;
        p.place = 0;
    }
    setPhase(Phase::Lobby);
    broadcast(lobbyMessage());
    goToLobbyScene();
}

void RaceSession::endRace() {
    if (!_host || _phase != Phase::Racing) return;
    for (RacePlayer& p : _players) {
        if (!statusIsFinal(p.status)) p.status = Status::GaveUp;
    }
    checkResults();
}

// ---- everyone --------------------------------------------------------------------------------------

void RaceSession::pickCharacter(int characterId) {
    if (_phase != Phase::Lobby || _level.forced) return;
    if (RacePlayer* me = mutablePlayer(_localId)) me->character = characterId;
    if (_host) broadcastLobby();
    else {
        sendToHost(net::Message("pick").set("char", characterId));
        changed();
    }
}

void RaceSession::setReady(bool ready) {
    if (_phase != Phase::Lobby) return;
    if (RacePlayer* me = mutablePlayer(_localId)) me->ready = ready;
    if (_host) broadcastLobby();
    else {
        sendToHost(net::Message("ready").set("ready", ready));
        changed();
    }
}

void RaceSession::giveUp() {
    if (_phase != Phase::Racing || statusIsFinal(_localStatus)) return;
    localStatus(Status::GaveUp);
}

void RaceSession::leave() {
    if (_phase == Phase::Idle) return;
    if (_host) {
        broadcast(net::Message("end").set("reason", "closed"));
        log("race: closed by the host");
    } else {
        sendToHost(net::Message("bye"));
        log("race: left");
    }
    reset();
}

void RaceSession::localStatus(Status s, int timeMs) {
    if (statusIsFinal(_localStatus) && s != Status::Left) return;
    _localStatus = s;
    if (_host) {
        hostSetStatus(_localId, s, timeMs);
    } else {
        if (RacePlayer* me = mutablePlayer(_localId)) {
            me->status = s;
            if (s == Status::Finished) me->timeMs = timeMs;
        }
        net::Message m("st");
        m.set("status", statusName(s));
        m.set("time", timeMs);
        sendToHost(m);
        changed();
    }
}

void RaceSession::reset() {
    for (auto& kv : _conns) {
        if (kv.second.channel) {
            kv.second.channel->clearHandler();
            kv.second.channel->close();
        }
    }
    _conns.clear();
    for (auto& kv : _pending) {
        if (kv.second.channel) {
            kv.second.channel->clearHandler();
            kv.second.channel->abort();
        }
    }
    _pending.clear();
    _invites.clear();
    if (_hostChannel) {
        _hostChannel->clearHandler();
        _hostChannel->close();
        _hostChannel = nullptr;
    }
    _players.clear();
    _tracks.clear();
    _fadeOut.clear();
    _capture.end();
    if (_frozen) {
        if (Gameplay* g = findGameplay(Director::getInstance()->getRunningScene())) {
            if (g == _gameplay) g->resume();
        }
        _frozen = false;
    }
    if (_hud && _hud->getParent()) _hud->removeFromParent();
    _hud = nullptr;
    if (_ghostLayer && _ghostLayer->getParent()) _ghostLayer->removeFromParent();
    _ghostLayer = nullptr;
    _gameplay = nullptr;
    RefPtr<Node> lobby = _lobbyPanel, results = _resultsPanel;
    _lobbyPanel = nullptr;   // cleared first: dismissing calls back into panelClosed
    _resultsPanel = nullptr;
    if (lobby && lobby->getParent()) static_cast<net::ui::Modal*>(lobby.get())->dismiss();
    if (results && results->getParent()) static_cast<net::ui::Modal*>(results.get())->dismiss();
    _phase = Phase::Idle;
    _host = false;
    _localId = 0;
    _localStatus = Status::Lobby;
    _level = RaceLevel();
    _hostName.clear();
    _sentLoaded = false;
}

void RaceSession::panelClosed(LobbyPanel* panel) {
    if (_lobbyPanel.get() != panel) return;
    _lobbyPanel = nullptr;
    if (_phase == Phase::Lobby) leave();
}

void RaceSession::resultsClosed(ResultsPanel* panel) {
    if (_resultsPanel.get() == panel) _resultsPanel = nullptr;
}

// ---- guest -------------------------------------------------------------------------------------------

void RaceSession::acceptIncoming(const std::shared_ptr<net::Channel>& channel, const net::Message& hello) {
    ensureTicking();
    if (hello.get("app") != "OpenWheels" || hello.getInt("proto", -1) != kRaceProtocol) {
        channel->send(net::Message("error").set("reason", "protocol"));
        channel->close();
        return;
    }
    const int id = _nextIncoming++;
    IncomingInvite in;
    in.channel = channel;
    in.hostName = net::LevelTransfer::sanitizeName(hello.get("name"));
    in.hostDevice = net::LevelTransfer::sanitizeName(hello.get("dev"));
    if (in.hostName.empty()) in.hostName = net::ipToString(channel->remoteAddress());
    in.deadline = now() + 60.0;
    _incoming[id] = in;
    channel->setMaxPayload(net::kMaxLevelBytes + 256 * 1024);
    net::Channel::Handler h;
    h.onMessage = [this, id](const net::Message& m) { incomingMessage(id, m); };
    h.onClosed = [this, id](const std::string&) { incomingClosed(id); };
    channel->setHandler(h);
    channel->send(helloMessage());
}

void RaceSession::incomingMessage(int id, const net::Message& m) {
    auto it = _incoming.find(id);
    if (it == _incoming.end()) return;
    IncomingInvite& in = it->second;
    if (m.type == "invite" && !in.gotInvite) {
        in.gotInvite = true;
        in.levelName = m.get("level");
        in.kind = m.get("kind");
        in.players = static_cast<int>(m.getInt("players", 1));
        if (_phase != Phase::Idle || (_askingIncoming != 0 && _askingIncoming != id)) {
            in.channel->send(net::Message("answer").set("accept", false).set("reason", "busy"));
            in.channel->close();
            in.channel->clearHandler();
            _incoming.erase(it);
            return;
        }
        _askingIncoming = id;
        return;
    }
    if (m.type == "end") {
        in.channel->close();
        incomingClosed(id);
    }
}

void RaceSession::incomingClosed(int id) {
    auto it = _incoming.find(id);
    if (it == _incoming.end()) return;
    it->second.channel->clearHandler();
    _incoming.erase(it);
    if (_askingIncoming == id) {
        _askingIncoming = 0;
        if (_inviteWindow) {
            _inviteWindow->dismissWindow(true);
        }
    }
}

void RaceSession::showIncoming() {
    auto it = _incoming.find(_askingIncoming);
    if (it == _incoming.end() || it->second.shown) return;
    IncomingInvite& in = it->second;
    log("race: invite from %s", in.hostName.c_str());
    if (!_autoSpec.empty() && _autoSpec == "join") {
        in.shown = true;
        answerIncoming(true);
        return;
    }
    std::string from = in.hostName;
    if (!in.hostDevice.empty() && in.hostDevice != in.hostName) from += " (" + in.hostDevice + ")";
    std::string message = from + " invites you to a ghost race on\n\xE2\x80\x9C" + in.levelName + "\xE2\x80\x9D";
    if (in.kind == "online") message += "\n(an online level)";
    InviteDelegate::get()->_onAnswer = [this](HWWindow* window, bool accept) {
        if (window == _inviteWindow) {
            _inviteWindow->release();
            _inviteWindow = nullptr;
            answerIncoming(accept);
        }
    };
    HWWindow* w = levelui::showAlert(kInviteAlertTag, "Race Invite", message, "Decline", "Join", InviteDelegate::get());
    if (!w) return;
    net::ui::greyButton(w, 0);
    w->retain();
    _inviteWindow = w;
    in.shown = true;
}

void RaceSession::answerIncoming(bool accept) {
    auto it = _incoming.find(_askingIncoming);
    _askingIncoming = 0;
    if (it == _incoming.end()) return;
    IncomingInvite in = it->second;
    _incoming.erase(it);
    if (!accept || _phase != Phase::Idle) {
        in.channel->send(net::Message("answer").set("accept", false).set("reason", accept ? "busy" : "declined"));
        in.channel->clearHandler();
        in.channel->close();
        return;
    }
    reset();
    _host = false;
    _hostChannel = in.channel;
    _hostName = in.hostName;
    _level = RaceLevel();
    _level.name = in.levelName;
    _level.kind = in.kind;
    net::Channel::Handler h;
    h.onMessage = [this](const net::Message& m) { guestMessage(m); };
    h.onClosed = [this](const std::string& error) { guestClosed(error); };
    _hostChannel->setHandler(h);
    net::Message answer("answer");
    answer.set("accept", true);
#if CC_TARGET_PLATFORM == CC_PLATFORM_ANDROID
    answer.set("plat", "phone");
#else
    answer.set("plat", "pc");
#endif
    _hostChannel->send(answer);
    _phase = Phase::Lobby;
    log("race: joined %s's race on \"%s\"", _hostName.c_str(), in.levelName.c_str());
}

void RaceSession::guestMessage(const net::Message& m) {
    if (m.type == "s") {
        receiveSnapshot(static_cast<int>(m.getInt("f")), m.get("d"));
        return;
    }
    if (m.type == "welcome") {
        _localId = static_cast<int>(m.getInt("you"));
        _autoReadyAt = now() + 1.0;
        return;
    }
    if (m.type == "lobby") {
        const Phase hostPhase = phaseFromName(m.get("phase"));
        const bool first = _players.empty();
        // keep our own pick / ready while the host has not seen them yet
        readLobby(m);
        if (first && _localId != 0 && !_level.forced) {
            int c = Settings::getInstance()->getSelectedCharacterId();
            if (_autoCharacter > 0) c = _autoCharacter;
            pickCharacter(c);
        }
        if (hostPhase == Phase::Lobby && _phase != Phase::Lobby) {
            setPhase(Phase::Lobby);
            goToLobbyScene();
        }
        if (const RacePlayer* me = localPlayer()) {
            if (statusIsFinal(me->status)) _localStatus = me->status;
        }
        changed();
        return;
    }
    if (m.type == "load") {
        _level = readLevel(m);
        _raceSerial = static_cast<int>(m.getInt("race"));
        setPhase(Phase::Loading);
        startLocalLevel();
        return;
    }
    if (m.type == "go") {
        _goAt = now() + m.getInt("delay", 3000) / 1000.0;
        setPhase(Phase::Countdown);
        return;
    }
    if (m.type == "results") {
        setPhase(Phase::Results);
        return;
    }
    if (m.type == "left") {
        playerLeft(static_cast<int>(m.getInt("id")), m.get("name"));
        return;
    }
    if (m.type == "end") {
        const std::string host = _hostName;
        const std::string reason = m.get("reason");
        reset();
        levelui::showAlert(0, "Race Over", reason == "full" ? "The race is full." : host + " closed the race.", "OK", "",
                           nullptr);
        return;
    }
}

void RaceSession::guestClosed(const std::string& error) {
    if (_phase == Phase::Idle) return;
    const std::string host = _hostName;
    if (!error.empty()) log("race: lost the host (%s)", error.c_str());
    reset();
    levelui::showAlert(0, "Race Over", "The connection to " + host + " was lost.", "OK", "", nullptr);
}

// ---- level start and scene integration ---------------------------------------------------------------

void RaceSession::leaveGameplay() {
    Scene* running = Director::getInstance()->getRunningScene();
    if (Gameplay* g = findGameplay(running)) {
        if (_frozen) g->resume();
        g->removeBanner(true);
        g->die();
    }
    _frozen = false;
    _gameplay = nullptr;
    _hud = nullptr;
    _ghostLayer = nullptr;
    _capture.end();
}

void RaceSession::startLocalLevel() {
    leaveGameplay();
    for (auto& kv : _tracks) kv.second.reset();
    _fadeOut.clear();
    _levelSeenFrames = 0;
    _haveStart = false;
    _haveFinish = false;
    _sentLoaded = false;
    _localStatus = Status::Loading;
    _resultsPanel = nullptr;
    _lobbyPanel = nullptr;

    const RacePlayer* me = localPlayer();
    const int character = _level.forced ? _level.forcedCharacter : (me ? me->character : 2);
    Settings* settings = Settings::getInstance();
    LevelSession* session = LevelSession::getInstance();
    Scene* scene = nullptr;
    if (_level.kind == "campaign") {
        session->clearLevelData();
        session->setChapterIndex(_level.chapter);
        settings->setSelectedLevel(_level.chapter, _level.level);
        settings->setSelectedCharacterId(character);
        scene = Gameplay::createScene("", nullptr);
    } else {
        session->clearLevelData();
        session->setChapterIndex(LevelStoreChapterImported);
        session->setLevelDataXML(_level.xml);
        session->setForceCharacter(_level.forced);
        session->setCharacterIndex(character);
        session->setVehicleIndex(0);
        session->applyToSettings();
        settings->setSelectedCharacterId(character);
        scene = Gameplay::createScene(settings->getSelectedLevelFilePath(), nullptr);
    }
    log("race: loading \"%s\" as character %d", _level.name.c_str(), character);
    Director::getInstance()->replaceScene(TransitionFade::create(0.4f, scene, Color3B::BLACK));
}

void RaceSession::goToLobbyScene() {
    leaveGameplay();
    for (auto& kv : _tracks) kv.second.reset();
    _localStatus = Status::Lobby;
    _resultsPanel = nullptr;
    _lobbyPanel = nullptr;
    LevelSession::getInstance()->leaveUserLevel();
    Director::getInstance()->replaceScene(
        TransitionFade::create(0.4f, MainMenu::createScene(MenuModeMain, nullptr), Color3B::BLACK));
}

void RaceSession::onLevelComplete() {
    if (_phase != Phase::Racing || statusIsFinal(_localStatus)) return;
    const int ms = static_cast<int>(std::lround(raceClock() * 1000.0));
    log("race: finished in %.3f s", ms / 1000.0);
    localStatus(Status::Finished, ms);
}

void RaceSession::onCharacterEvent(const std::string& name) {
    if (_phase != Phase::Racing || statusIsFinal(_localStatus)) return;
    if (name == "dead") localStatus(Status::Dead);
    else if (name == "ejected" && _localStatus == Status::Racing) localStatus(Status::Ejected);
}

void RaceSession::integrateScene() {
    Scene* scene = Director::getInstance()->getRunningScene();
    if (!scene || dynamic_cast<TransitionScene*>(scene)) return;
    Gameplay* g = findGameplay(scene);
    if (!g) {
        _gameplay = nullptr;
        if (_phase == Phase::Racing && !statusIsFinal(_localStatus) && sceneHasMainMenu(scene)) {
            localStatus(Status::GaveUp);   // left the level through the pause menu
        }
        return;
    }
    if (_phase == Phase::Idle || _phase == Phase::Lobby) return;

    if (!g->getChildByName("ow_race_hud")) {
        // a new Gameplay: the first one of this race, or a restart / character change
        RaceHud* hud = RaceHud::create();
        g->addChild(hud, 50);
        _hud = hud;
        _gameplay = g;
        _ghostLayer = nullptr;
        _capture.end();
        _frozen = false;
        _levelSeenFrames = 0;
        _haveStart = false;
        ++_run;
        if (_phase == Phase::Racing && !statusIsFinal(_localStatus)) localStatus(Status::Racing);
    }
    if (g != _gameplay) return;
    LevelB2D* level = g->getLevel();
    Session* session = Settings::getInstance()->getCurrentSession();
    if (!level || !session) return;
    if (!_ghostLayer || !_ghostLayer->getParent()) {
        GhostLayer* gl = GhostLayer::create();
        session->addChild(gl, 5);   // behind the rider layers (6..12), in front of shapes and items
        _ghostLayer = gl;
        _capture.begin(session, _run);
        _haveFinish = false;
        int misses = 0;
        for (unsigned int i = 0; misses < 200 && i < 5000; ++i) {
            LevelItem* item = level->getSpecial(i);
            if (!item) {
                ++misses;
                continue;
            }
            misses = 0;
            if (FinishLine* f = dynamic_cast<FinishLine*>(item)) {
                if (Node* art = FinishLineAccess::art(f)) {
                    _finishPos = art->getPosition();
                    _haveFinish = true;
                }
                break;
            }
        }
    }
    ++_levelSeenFrames;
    if (CharacterB2D* c = level->getCharacter()) {
        if (b2Body* focus = c->getFocus()) {
            const float ptm = session->getPtmRatio();
            _localFocus = Vec2(focus->GetPosition().x * ptm, focus->GetPosition().y * ptm);
            if (!_haveStart) {
                _startFocus = _localFocus;
                _haveStart = true;
            }
        }
    }
    if (_phase == Phase::Loading || _phase == Phase::Countdown) {
        if (!_frozen && _levelSeenFrames >= 4) {
            g->pause();   // held until GO (camera and background have placed themselves by now)
            _frozen = true;
        }
        if (_frozen && _phase == Phase::Loading && !_sentLoaded) {
            _sentLoaded = true;
            _localStatus = Status::Loaded;
            if (_host) {
                _loaded[_localId] = true;
                if (RacePlayer* me = mutablePlayer(_localId)) me->status = Status::Loaded;
                broadcastLobby();
            } else {
                sendToHost(net::Message("loaded"));
            }
        }
    } else if (_frozen) {
        g->resume();
        _frozen = false;
    }
}

void RaceSession::captureAndSend(Gameplay* g) {
    if (!_capture.active() || !g) return;
    if (_phase != Phase::Countdown && _phase != Phase::Racing && _phase != Phase::Results) return;
    const double t = now();
    if (_lastCapture >= 0.0 && t - _lastCapture < kSnapshotInterval - 0.004) return;
    _lastCapture = t;
    const std::string data = _capture.capture(t, _localFocus);
    if (data.empty()) return;
    net::Message m("s");
    m.set("d", data);
    if (_host) {
        m.set("f", _localId);
        broadcast(m);
    } else {
        sendToHost(m);
    }
    _txBytes += data.size() + 32;   // + framing (16-byte header, type and keys)
    ++_txSnaps;
    if (t - _statsSince >= 5.0) {
        log("race: ghost upload %.1f KB/s (%.1f snapshots/s, %d nodes)", _txBytes / 1024.0 / (t - _statsSince),
            _txSnaps / (t - _statsSince), static_cast<int>(_capture.nodeCount()));
        for (const auto& kv : _tracks) {
            log("race: ghost %d received %u snapshots, %u bytes", kv.first, static_cast<unsigned>(kv.second.snapshots),
                static_cast<unsigned>(kv.second.bytes));
        }
        _txBytes = 0;
        _txSnaps = 0;
        _statsSince = t;
    }
}

void RaceSession::receiveSnapshot(int fromId, const std::string& data) {
    if (fromId == _localId || fromId == 0) return;
    if (!player(fromId)) return;
    _tracks[fromId].apply(data, now());
}

void RaceSession::drawGhosts() {
    GhostLayer* layer = dynamic_cast<GhostLayer*>(_ghostLayer.get());
    if (!layer || !layer->getParent()) return;
    const double t = now();
    for (auto it = _tracks.begin(); it != _tracks.end();) {
        const RacePlayer* p = player(it->first);
        float fade = 1.0f;
        auto f = _fadeOut.find(it->first);
        if (f != _fadeOut.end()) fade = 1.0f - static_cast<float>((t - f->second) / 1.5);
        if (!p || fade <= 0.0f) {
            it = _tracks.erase(it);
            continue;
        }
        GhostStyle style;
        style.tint = slotColor(p->slot);
        style.name = p->name;
        layer->drawTrack(it->first, it->second, style, t, fade);
        ++it;
    }
    layer->sweep();
}

// ---- tick ----------------------------------------------------------------------------------------------

void RaceSession::tick(float) {
    if (_phase == Phase::Idle && _incoming.empty() && _autoSpec.empty()) return;
    const double t = now();

    // invites on the guest side
    if (_askingIncoming != 0 && !_inviteWindow && net::LevelTransfer::canInterruptNow()) showIncoming();
    for (auto it = _incoming.begin(); it != _incoming.end();) {
        if (t > it->second.deadline) {
            it->second.channel->send(net::Message("answer").set("accept", false).set("reason", "timeout"));
            it->second.channel->clearHandler();
            it->second.channel->close();
            if (_askingIncoming == it->first) {
                _askingIncoming = 0;
                if (_inviteWindow) _inviteWindow->dismissWindow(true);
            }
            it = _incoming.erase(it);
        } else {
            ++it;
        }
    }
    // invite timeouts on the host side
    for (auto it = _pending.begin(); it != _pending.end();) {
        if (t > it->second.deadline) {
            it->second.channel->clearHandler();
            it->second.channel->abort();
            _invites[it->first] = InviteInfo{InviteInfo::Failed, "No answer"};
            it = _pending.erase(it);
            changed();
        } else {
            ++it;
        }
    }

    if (!_autoSpec.empty()) autoTestTick();
    if (_phase == Phase::Idle) return;

    // host: everybody loaded (or the slow ones timed out) -> countdown
    if (_host && _phase == Phase::Loading) {
        bool all = true;
        for (const RacePlayer& p : _players) {
            if (p.status != Status::Left && !_loaded[p.id]) all = false;
        }
        if (all || t - _loadStarted > kLoadTimeout) {
            _goAt = t + kCountdownSeconds;
            broadcast(net::Message("go").set("delay", static_cast<int>(kCountdownSeconds * 1000)));
            setPhase(Phase::Countdown);
            broadcastLobby();
        }
    }
    if (_phase == Phase::Countdown && t >= _goAt) {
        setPhase(Phase::Racing);
        localStatus(Status::Racing);
    }

    integrateScene();
    Gameplay* g = findGameplay(Director::getInstance()->getRunningScene());
    if (g && g == _gameplay) {
        captureAndSend(g);
        drawGhosts();
        if (RaceHud* hud = dynamic_cast<RaceHud*>(_hud.get())) hud->refresh();
    }

    // panels
    Scene* scene = Director::getInstance()->getRunningScene();
    const bool sceneReady = scene && !dynamic_cast<TransitionScene*>(scene);
    if (sceneReady && _phase == Phase::Lobby && (!findGameplay(scene) || net::LevelTransfer::canInterruptNow()) &&
        (!_lobbyPanel || !_lobbyPanel->getParent()) && !online::ui::modalOpen()) {
        LobbyPanel* p = LobbyPanel::create();
        _lobbyPanel = p;
        p->present();
    }
    if (sceneReady && _phase == Phase::Results && !_resultsShown) {
        _resultsShown = true;
        ResultsPanel* p = ResultsPanel::create();
        _resultsPanel = p;
        p->present();
    }
}

void RaceSession::autoTestTick() {
    Scene* scene = Director::getInstance()->getRunningScene();
    if (_autoSpec.compare(0, 5, "host:") == 0) {
        if (!_autoHosted && sceneHasMainMenu(scene) && !dynamic_cast<TransitionScene*>(scene)) {
            _autoHosted = true;
            RaceLevel l;
            const std::vector<std::string> f = split(_autoSpec, ':');
            if (f.size() >= 4 && f[1] == "campaign") {
                l.kind = "campaign";
                l.chapter = std::atoi(f[2].c_str());
                l.level = std::atoi(f[3].c_str());
            } else if (f.size() >= 3 && (f[1] == "online" || f[1] == "level")) {
                const std::string path = _autoSpec.substr(f[0].size() + f[1].size() + 2);
                std::string xml = FileUtils::getInstance()->getStringFromFile(path);
                if (f[1] == "online") {
                    online::ConversionReport report;
                    xml = online::FlashLevelConverter::toMobile(xml, &report);
                    l.forced = report.forceCharacter;
                    l.forcedCharacter = report.character;
                    l.kind = "online";
                } else {
                    l.kind = "mobile";
                }
                l.xml = xml;
                std::string name = path.substr(path.find_last_of("/\\") + 1);
                l.name = name.substr(0, name.find_last_of('.'));
            }
            hostLevel(l);
        }
        if (_host && _phase == Phase::Lobby) {
            for (const net::Peer& p : net::LanDiscovery::getInstance()->peers()) {
                if (!p.offers(kRaceService)) continue;
                if (_invites.count(p.id)) continue;
                if (static_cast<int>(_players.size() + _pending.size()) >= _autoPlayers) break;
                invite(p);
            }
            if (static_cast<int>(_players.size()) >= _autoPlayers && canStart(nullptr)) start();
        }
    } else if (_autoSpec == "join") {
        if (_inviteWindow) {   // the invite arrived before the test hook was installed
            HWWindow* w = _inviteWindow;
            _inviteWindow = nullptr;
            w->dismissWindow(true);
            w->release();
            answerIncoming(true);
        }
        if (_phase == Phase::Lobby && _localId != 0 && _autoReadyAt > 0.0 && now() > _autoReadyAt) {
            _autoReadyAt = 0.0;
            setReady(true);
        }
    }
}

}  // namespace race
