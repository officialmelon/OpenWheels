// NET (PC addition): level transfer between nearby players, see LevelTransfer.h.
#include "net/LevelTransfer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "cocos2d.h"

#include "CharacterSelectLayer.h"
#include "EditorLayer.h"
#include "EditorViewController.h"
#include "Gameplay.h"
#include "HWWindow.h"
#include "HWWindowDelegate.h"
#include "LevelMO.h"
#include "LevelSession.h"
#include "LevelStore.h"
#include "LevelUIHelpers.h"
#include "PauseLayer.h"
#include "online/FlashLevelConverter.h"
#include "net/NetUi.h"
#include "net/race/RaceSession.h"  // NET (PC addition): ghost race

USING_NS_CC;

namespace net {

namespace {

constexpr const char* kPlayerNameKey = "net_player_name";
constexpr const char* kCodeAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";   // Crockford base 32
constexpr double kHelloTimeout = 15.0;
constexpr double kAskTimeout = 60.0;
constexpr double kLevelTimeout = 90.0;
constexpr double kSenderWaitTimeout = 75.0;
constexpr double kSenderResultTimeout = 90.0;
constexpr double kOfferSpacing = 3.0;
constexpr double kDeclineCooldown = 10.0;
constexpr size_t kOffersPerMinute = 10;
constexpr size_t kMaxPendingConnections = 6;

enum AlertTag {
    kTagOffer = 7101,
    kTagReceived = 7102,
    kTagInfo = 7103,
};

std::string truncateUtf8(const std::string& s, size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    size_t n = maxBytes;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xc0) == 0x80) --n;   // don't split a character
    return s.substr(0, n);
}

Message helloMessage() {
    Message m("hello");
    m.set("app", "OpenWheels");
    m.set("proto", kTransferProtocol);
    m.set("purpose", "level");
    m.set("name", LevelTransfer::getInstance()->playerName());
    m.set("dev", deviceName());
    m.set("id", LanDiscovery::getInstance()->instanceId());
    return m;
}

bool validHello(const Message& m) {
    return m.get("app") == "OpenWheels" && m.getInt("proto", -1) == kTransferProtocol;
}

}  // namespace

// ---- alert delegate --------------------------------------------------------------------------

class TransferAlertDelegate : public HWWindowDelegate {
public:
    static TransferAlertDelegate* get() {
        static TransferAlertDelegate delegate;
        return &delegate;
    }

    void hwWindowButtonPressed(int buttonTag, HWWindow* window) override {
        LevelTransfer* t = LevelTransfer::getInstance();
        const long index = levelui::buttonIndex(buttonTag, window);
        if (window->getTag() == kTagOffer) {
            if (window == t->_offerWindow) t->answerOffer(t->_askingId, index == 1);
        } else if (window->getTag() == kTagReceived) {
            RefPtr<LevelMO> level = _received;
            _received = nullptr;
            if (index == 1 && level) playLevel(level.get());
        }
    }

    void hwWindowWasDismissed(HWWindow* window) override {
        LevelTransfer* t = LevelTransfer::getInstance();
        if (window == t->_offerWindow) {
            t->_offerWindow->release();
            t->_offerWindow = nullptr;
        }
    }

    // As UserLevelSelectUIView::playBtnPressed, but the level replaces the running scene (like
    // -[AppController playImportedLevel]); a paused gameplay is left like its EXIT button does.
    static void playLevel(LevelMO* level) {
        Scene* running = Director::getInstance()->getRunningScene();
        if (running) {
            for (Node* child : running->getChildren()) {
                if (Gameplay* gameplay = dynamic_cast<Gameplay*>(child)) {
                    gameplay->removeBanner(true);
                    gameplay->die();
                }
            }
        }
        LevelSession* session = LevelSession::getInstance();
        session->setChapterIndex(LevelStoreChapterUser);
        session->setLevelDataWithManagedObject(level);
        const bool force = level->force_character();
        session->setCharacterIndex(level->playable_character());
        session->setVehicleIndex(0);
        session->applyToSettings();
        Scene* scene = force ? Gameplay::createScene(session->levelDataXML(), nullptr)
                             : CharacterSelectLayer::createScene(0, 0);
        EditorViewController::dismissRootPresented(false, nullptr);
        if (running) Director::getInstance()->replaceScene(scene);
        else Director::getInstance()->pushScene(scene);
    }

    RefPtr<LevelMO> _received;
};

// ---- state -----------------------------------------------------------------------------------

struct LevelTransfer::Incoming {
    enum class State { Hello, Offer, Pending, Asking, AwaitLevel, Done };
    int id = 0;
    std::shared_ptr<Channel> channel;
    State state = State::Hello;
    double deadline = 0.0;
    std::string fromName, fromDevice;
    std::string levelName, comments, kind;
    long long size = 0;
};

struct LevelTransfer::Outgoing {
    int id = 0;
    std::shared_ptr<Channel> channel;
    SendTarget target;
    LevelPackage level;
    SendCallback callback;
    SendState state = SendState::Connecting;
    double deadline = 0.0;
    bool gotHello = false;
};

LevelPackage LevelPackage::fromLevel(LevelMO* level) {
    LevelPackage p;
    if (!level) return p;
    p.name = level->name();
    p.comments = level->comments();
    p.data = level->data();
    p.playableCharacter = level->playable_character();
    p.forceCharacter = level->force_character();
    p.kind = "mobile";
    return p;
}

LevelTransfer* LevelTransfer::getInstance() {
    static LevelTransfer* instance = new LevelTransfer();
    return instance;
}

LevelTransfer::LevelTransfer() = default;

void LevelTransfer::start() {
    if (_started) return;
    _started = true;
    NetLoop::instance();   // the worker + scheduler pump, on the cocos thread
    _listener = Listener::open(kTransferPort, kTransferPortCount,
                               [this](const std::shared_ptr<Channel>& channel) { accept(channel); });
    if (_listener) {
        log("net: listening for levels on TCP %u", static_cast<unsigned>(_listener->port()));
        LanDiscovery::getInstance()->announce(playerName(), deviceName(), _listener->port(), {kLevelsService, race::kRaceService});  // NET: + ghost race
    } else {
        log("net: no free TCP port in %u..%u, receiving levels is off", static_cast<unsigned>(kTransferPort),
            static_cast<unsigned>(kTransferPort + kTransferPortCount - 1));
    }
    Director::getInstance()->getScheduler()->schedule([this](float dt) { tick(dt); }, this, 0.25f, false,
                                                      "net_level_transfer");
}

void LevelTransfer::shutdown() {
    if (!_started) return;
    if (_listener) _listener->close();
    LanDiscovery::getInstance()->stop();
    NetLoop::instance()->shutdown();
}

uint16_t LevelTransfer::port() const {
    return _listener ? _listener->port() : 0;
}

std::string LevelTransfer::sanitizeName(const std::string& name) {
    std::string s;
    for (char c : name) {
        if (static_cast<unsigned char>(c) >= 0x20 && c != 0x7f) s += c;
    }
    s = levelui::trimmed(s);
    return truncateUtf8(s, 32);
}

std::string LevelTransfer::playerName() const {
    std::string name = sanitizeName(UserDefault::getInstance()->getStringForKey(kPlayerNameKey, ""));
    return name.empty() ? sanitizeName(deviceName()) : name;
}

void LevelTransfer::setPlayerName(const std::string& name) {
    const std::string clean = sanitizeName(name);
    UserDefault::getInstance()->setStringForKey(kPlayerNameKey, clean);
    UserDefault::getInstance()->flush();
    if (_listener) {
        LanDiscovery::getInstance()->announce(playerName(), deviceName(), _listener->port(), {kLevelsService, race::kRaceService});  // NET: + ghost race
    }
}

// ---- receive code ----------------------------------------------------------------------------

std::string LevelTransfer::encodeCode(uint32_t address, uint16_t port) {
    const uint32_t offset = static_cast<uint32_t>(port - kTransferPort) & 0xfu;
    uint32_t check = offset;
    for (int i = 0; i < 8; ++i) check ^= (address >> (i * 4)) & 0xfu;
    check ^= 0x5u;
    const uint64_t bits = (static_cast<uint64_t>(address) << 8) | (offset << 4) | check;
    std::string code;
    for (int i = 7; i >= 0; --i) {
        code += kCodeAlphabet[(bits >> (i * 5)) & 31u];
        if (i == 4) code += '-';
    }
    return code;
}

bool LevelTransfer::parseTarget(const std::string& rawText, uint32_t* address, uint16_t* port) {
    const std::string text = levelui::trimmed(rawText);
    // a.b.c.d[:port]
    const size_t colon = text.find(':');
    uint32_t ip = 0;
    if (parseIPv4(text.substr(0, colon), &ip)) {
        long p = kTransferPort;
        if (colon != std::string::npos) {
            char* end = nullptr;
            p = std::strtol(text.c_str() + colon + 1, &end, 10);
            if (!end || *end != 0 || p <= 0 || p > 65535) return false;
        }
        if (address) *address = ip;
        if (port) *port = static_cast<uint16_t>(p);
        return true;
    }
    // code: 8 base-32 characters, separators ignored, O->0, I/L->1
    uint64_t bits = 0;
    int digits = 0;
    for (char c : text) {
        if (c == '-' || c == ' ') continue;
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if (c == 'O') c = '0';
        if (c == 'I' || c == 'L') c = '1';
        const char* pos = std::strchr(kCodeAlphabet, c);
        if (!pos || c == 0) return false;
        bits = (bits << 5) | static_cast<uint64_t>(pos - kCodeAlphabet);
        ++digits;
    }
    if (digits != 8) return false;
    ip = static_cast<uint32_t>(bits >> 8);
    const uint32_t offset = static_cast<uint32_t>(bits >> 4) & 0xfu;
    uint32_t check = offset;
    for (int i = 0; i < 8; ++i) check ^= (ip >> (i * 4)) & 0xfu;
    check ^= 0x5u;
    if ((bits & 0xfu) != check) return false;
    if (address) *address = ip;
    if (port) *port = static_cast<uint16_t>(kTransferPort + offset);
    return true;
}

std::string LevelTransfer::receiveCode() const {
    if (!_listener) return "";
    return encodeCode(primaryAddress(), _listener->port());
}

std::string LevelTransfer::receiveAddressText() const {
    if (!_listener) return "";
    return ipToString(primaryAddress()) + ":" + std::to_string(_listener->port());
}

// ---- popups ----------------------------------------------------------------------------------

bool LevelTransfer::canInterruptNow() {
    Scene* scene = Director::getInstance()->getRunningScene();
    if (!scene || dynamic_cast<TransitionScene*>(scene)) return false;
    for (Node* child : scene->getChildren()) {
        if (dynamic_cast<Gameplay*>(child)) {
            bool paused = false;
            for (Node* c : child->getChildren()) {
                if (dynamic_cast<PauseLayer*>(c)) paused = true;
            }
            if (!paused) return false;
        }
    }
    return true;
}

void LevelTransfer::showAlert(const std::string& title, const std::string& message) {
    levelui::showAlert(kTagInfo, title, message, "OK", "", nullptr);
}

// ---- receiving -------------------------------------------------------------------------------

void LevelTransfer::accept(const std::shared_ptr<Channel>& channel) {
    size_t waiting = 0;
    for (auto& kv : _incoming) {
        if (kv.second->state == Incoming::State::Hello || kv.second->state == Incoming::State::Offer) ++waiting;
    }
    if (waiting >= kMaxPendingConnections) {
        channel->abort();
        return;
    }
    std::unique_ptr<Incoming> in(new Incoming());
    in->id = _nextId++;
    in->channel = channel;
    in->deadline = NetLoop::now() + kHelloTimeout;
    const int id = in->id;
    channel->setMaxPayload(kMaxLevelBytes + 64 * 1024);
    Channel::Handler h;
    h.onMessage = [this, id](const Message& m) { onIncomingMessage(id, m); };
    h.onClosed = [this, id](const std::string& error) { onIncomingClosed(id, error); };
    _incoming[id] = std::move(in);
    channel->setHandler(h);
}

void LevelTransfer::onIncomingMessage(int id, const Message& m) {
    auto it = _incoming.find(id);
    if (it == _incoming.end()) return;
    Incoming& in = *it->second;
    if (m.type == "hello" && in.state == Incoming::State::Hello && m.get("purpose") == "race") {
        // NET (PC addition): a ghost-race invite; the race session takes the connection over.
        std::shared_ptr<Channel> channel = in.channel;
        finishIncoming(id);
        race::RaceSession::get()->acceptIncoming(channel, m);
        return;
    }
    if (m.type == "hello" && in.state == Incoming::State::Hello) {
        if (!validHello(m) || m.get("purpose") != "level") {
            in.channel->send(Message("error").set("reason", "protocol"));
            in.channel->close();
            finishIncoming(id);
            return;
        }
        in.fromName = sanitizeName(m.get("name"));
        in.fromDevice = sanitizeName(m.get("dev"));
        if (in.fromName.empty()) in.fromName = in.fromDevice.empty() ? ipToString(in.channel->remoteAddress()) : in.fromDevice;
        in.channel->send(helloMessage());
        in.state = Incoming::State::Offer;
        return;
    }
    if (m.type == "offer" && in.state == Incoming::State::Offer) {
        handleOffer(in, m);
        return;
    }
    if (m.type == "level" && in.state == Incoming::State::AwaitLevel) {
        handleLevel(in, m);
        return;
    }
    // Anything else is out of order.
    in.channel->abort();
    finishIncoming(id);
}

void LevelTransfer::handleOffer(Incoming& in, const Message& offer) {
    const double now = NetLoop::now();
    const uint32_t from = in.channel->remoteAddress();
    auto refuse = [&](const char* reason) {
        in.channel->send(Message("answer").set("accept", false).set("reason", reason));
        in.channel->close();
        finishIncoming(in.id);
    };
    in.levelName = truncateUtf8(levelui::trimmed(offer.get("name")), 100);
    if (in.levelName.empty()) in.levelName = "Untitled";
    in.comments = offer.get("comments");
    in.kind = offer.get("kind");
    in.size = offer.getInt("size", -1);
    if (in.kind != "mobile" && in.kind != "flash") return refuse("unsupported");
    if (in.size < 0 || in.size > static_cast<long long>(kMaxLevelBytes)) return refuse("too_large");

    // rate limits
    _recentOffers.erase(std::remove_if(_recentOffers.begin(), _recentOffers.end(),
                                       [now](double t) { return now - t > 60.0; }),
                        _recentOffers.end());
    auto last = _lastOfferFrom.find(from);
    const bool tooSoon = last != _lastOfferFrom.end() && now - last->second < kOfferSpacing;
    auto cooldown = _declinedUntil.find(from);
    const bool coolingDown = cooldown != _declinedUntil.end() && now < cooldown->second;
    if (tooSoon || coolingDown || _recentOffers.size() >= kOffersPerMinute) return refuse("rate");
    _lastOfferFrom[from] = now;
    _recentOffers.push_back(now);
    if (_askingId != 0) return refuse("busy");

    _askingId = in.id;
    in.state = Incoming::State::Pending;
    in.deadline = now + kAskTimeout;
    in.channel->setIdleTimeout(0.0);
    if (canInterruptNow()) showOffer(in);
}

void LevelTransfer::showOffer(Incoming& in) {
    std::string from = in.fromName;
    if (!in.fromDevice.empty() && in.fromDevice != in.fromName) from += " (" + in.fromDevice + ")";
    std::string message = StringUtils::format("%s wants to send you\n\xE2\x80\x9C%s\xE2\x80\x9D", from.c_str(),
                                              in.levelName.c_str());
    if (in.kind == "flash") message += "\n(an online level)";
    HWWindow* window = levelui::showAlert(kTagOffer, "Incoming Level", message, "Decline", "Accept",
                                          TransferAlertDelegate::get());
    if (!window) return;
    ui::greyButton(window, 0);   // Decline: grey next to the blue Accept
    window->retain();
    _offerWindow = window;
    in.state = Incoming::State::Asking;
}

void LevelTransfer::answerOffer(int id, bool accept) {
    auto it = _incoming.find(id);
    if (it == _incoming.end() || id != _askingId) {
        if (accept) showAlert("Transfer Failed", "The sender is gone. Ask them to send the level again.");
        return;
    }
    Incoming& in = *it->second;
    if (!accept) {
        _declinedUntil[in.channel->remoteAddress()] = NetLoop::now() + kDeclineCooldown;
        in.channel->send(Message("answer").set("accept", false).set("reason", "declined"));
        in.channel->close();
        _askingId = 0;
        finishIncoming(id);
        return;
    }
    in.channel->send(Message("answer").set("accept", true));
    in.state = Incoming::State::AwaitLevel;
    in.deadline = NetLoop::now() + kLevelTimeout;
}

void LevelTransfer::handleLevel(Incoming& in, const Message& m) {
    const int id = in.id;
    const std::string from = in.fromName;
    auto fail = [&](const std::string& error) {
        in.channel->send(Message("result").set("ok", false).set("error", error));
        in.channel->close();
        _askingId = 0;
        finishIncoming(id);
        showAlert("Transfer Failed", StringUtils::format("The level from %s could not be used:\n%s", from.c_str(),
                                                         error.c_str()));
    };
    std::string data = m.get("data");
    const std::string kind = m.get("kind").empty() ? in.kind : m.get("kind");
    if (data.size() > kMaxLevelBytes) return fail("the level is larger than 4 MB");
    int character = static_cast<int>(m.getInt("playable_character", 1));
    bool force = m.getBool("force_character");
    if (kind == "flash") {
        online::ConversionReport report;
        data = online::FlashLevelConverter::toMobile(data, &report);
        if (!report.ok) return fail(report.error.empty() ? "the online level could not be converted" : report.error);
        character = report.character;
        force = report.forceCharacter;
    } else if (kind != "mobile") {
        return fail("unknown level type");
    }
    std::string xmlError;
    if (!LevelStore::isValidLevelXML(data, &xmlError)) return fail("not a level (" + xmlError + ")");

    // Save into the imported chapter (5001, LevelStore's import path, like a .happywheels file):
    // the user-level screen lists it under "Received", the editor's Load Level under imported.
    LevelStore* store = LevelStore::getInstance();
    ChapterMO* chapter = store->chapterWithIndex(LevelStoreChapterImported);
    if (!chapter) return fail("your levels could not be opened");
    std::string name = truncateUtf8(levelui::trimmed(m.get("name")), 100);
    if (name.empty()) name = in.levelName;
    std::string unique = name;
    for (int n = 2; n < 1000; ++n) {
        bool taken = false;
        for (LevelMO* l : chapter->levels()) {
            if (l->name() == unique) taken = true;
        }
        if (!taken) break;
        unique = StringUtils::format("%s (%d)", name.c_str(), n);
    }
    LevelMO* level = store->insertNewLevel();
    level->setId_x(store->lastLevelId(LevelStoreChapterImported) + 1);
    level->setName(unique);
    level->setComments(truncateUtf8(m.get("comments"), 4096));
    level->setData(data);
    level->setForce_character(force);
    level->setPlayable_character(character);
    chapter->addLevelsObject(level);
    std::string saveError;
    if (!store->save(&saveError)) {
        store->deleteObject(level);
        return fail("saving failed (" + saveError + ")");
    }
    in.channel->send(Message("result").set("ok", true).set("saved", unique));
    in.channel->close();
    _askingId = 0;
    finishIncoming(id);
    log("net: received \"%s\" from %s (%u bytes)", unique.c_str(), from.c_str(), static_cast<unsigned>(data.size()));
    showReceived(level, from);
}

void LevelTransfer::showReceived(LevelMO* level, const std::string& from) {
    Scene* scene = Director::getInstance()->getRunningScene();
    bool inEditor = false;
    if (scene) {
        for (Node* child : scene->getChildren()) {
            if (dynamic_cast<EditorLayer*>(child)) inEditor = true;
        }
    }
    if (inEditor) {
        // Playing would replace the editor (and lose unsaved work): just tell where it is.
        showAlert("Level Received",
                  StringUtils::format("\xE2\x80\x9C%s\xE2\x80\x9D from %s is saved in your levels. Find it under "
                                      "Load Level, Imported.",
                                      level->name().c_str(), from.c_str()));
        return;
    }
    TransferAlertDelegate::get()->_received = level;
    HWWindow* window = levelui::showAlert(
        kTagReceived, "Play Now?",
        StringUtils::format("\xE2\x80\x9C%s\xE2\x80\x9D from %s is saved in your levels.", level->name().c_str(),
                            from.c_str()),
        "Later", "Play", TransferAlertDelegate::get());
    ui::greyButton(window, 0);   // Later: grey next to the blue Play
}

void LevelTransfer::onIncomingClosed(int id, const std::string& error) {
    auto it = _incoming.find(id);
    if (it == _incoming.end()) return;
    Incoming& in = *it->second;
    const bool wasActive = id == _askingId;
    const Incoming::State state = in.state;
    const std::string from = in.fromName;
    if (wasActive) {
        _askingId = 0;
        if (_offerWindow) {
            _offerWindow->dismissWindow(true);
        }
    }
    finishIncoming(id);
    if (wasActive && (state == Incoming::State::Asking || state == Incoming::State::AwaitLevel)) {
        showAlert("Transfer Cancelled", from + " cancelled sending the level.");
    }
}

void LevelTransfer::finishIncoming(int id) {
    auto it = _incoming.find(id);
    if (it == _incoming.end()) return;
    it->second->channel->clearHandler();
    // Erase later: we may be inside one of the channel's callbacks.
    std::shared_ptr<Channel> keep = it->second->channel;
    _incoming.erase(it);
    NetLoop::instance()->post([keep]() {});
}

// ---- sending ---------------------------------------------------------------------------------

int LevelTransfer::send(const SendTarget& target, const LevelPackage& level, SendCallback callback) {
    std::unique_ptr<Outgoing> out(new Outgoing());
    out->id = _nextId++;
    out->target = target;
    if (out->target.name.empty()) out->target.name = ipToString(target.address);
    out->level = level;
    out->callback = callback;
    out->deadline = NetLoop::now() + 15.0;
    const int id = out->id;
    if (level.data.size() > kMaxLevelBytes) {
        if (callback) callback(SendState::Failed, "This level is too large to send (more than 4 MB).");
        return 0;
    }
    out->channel = Channel::connect(target.address, target.port);
    Channel::Handler h;
    h.onOpen = [this, id]() {
        auto it = _outgoing.find(id);
        if (it == _outgoing.end()) return;
        Outgoing& o = *it->second;
        o.channel->send(helloMessage());
        Message offer("offer");
        offer.set("name", o.level.name);
        offer.set("comments", truncateUtf8(o.level.comments, 4096));
        offer.set("kind", o.level.kind);
        offer.set("size", static_cast<long long>(o.level.data.size()));
        offer.set("character", o.level.playableCharacter);
        offer.set("force", o.level.forceCharacter);
        o.channel->send(offer);
        o.state = SendState::Waiting;
        o.deadline = NetLoop::now() + kSenderWaitTimeout;
        if (o.callback) o.callback(SendState::Waiting, "Waiting for " + o.target.name + " to accept\xE2\x80\xA6");
    };
    h.onMessage = [this, id](const Message& m) { onOutgoingMessage(id, m); };
    h.onClosed = [this, id](const std::string& error) { onOutgoingClosed(id, error); };
    _outgoing[id] = std::move(out);
    if (callback) callback(SendState::Connecting, "Connecting to " + _outgoing[id]->target.name + "…");
    _outgoing[id]->channel->setHandler(h);
    return id;
}

void LevelTransfer::cancelSend(int sendId) {
    auto it = _outgoing.find(sendId);
    if (it == _outgoing.end()) return;
    it->second->callback = nullptr;
    it->second->channel->abort();
    finishOutgoing(sendId, SendState::Failed, "");
}

void LevelTransfer::onOutgoingMessage(int id, const Message& m) {
    auto it = _outgoing.find(id);
    if (it == _outgoing.end()) return;
    Outgoing& o = *it->second;
    const std::string& who = o.target.name;
    if (m.type == "hello") {
        if (!validHello(m)) {
            o.channel->abort();
            finishOutgoing(id, SendState::Failed, who + " runs a different version of OpenWheels.");
            return;
        }
        o.gotHello = true;
        const std::string name = sanitizeName(m.get("name"));
        if (!name.empty() && name != o.target.name) {
            o.target.name = name;   // a code target is known by its address until now
            if (o.state == SendState::Waiting && o.callback) {
                o.callback(SendState::Waiting, "Waiting for " + name + " to accept\xE2\x80\xA6");
            }
        }
        return;
    }
    if (m.type == "error") {
        o.channel->abort();
        finishOutgoing(id, SendState::Failed, who + " runs a different version of OpenWheels.");
        return;
    }
    if (m.type == "answer" && o.state == SendState::Waiting) {
        if (!m.getBool("accept")) {
            const std::string reason = m.get("reason");
            std::string text;
            SendState state = SendState::Failed;
            if (reason == "declined") {
                text = who + " declined the level.";
                state = SendState::Declined;
            } else if (reason == "busy") {
                text = who + " is busy with another level. Try again in a moment.";
            } else if (reason == "rate") {
                text = "Please wait a few seconds before sending to " + who + " again.";
            } else if (reason == "too_large") {
                text = "This level is too large to send (more than 4 MB).";
            } else if (reason == "timeout") {
                text = who + " did not answer.";
            } else {
                text = who + " cannot take this level.";
            }
            o.channel->close();
            finishOutgoing(id, state, text);
            return;
        }
        Message level("level");
        level.set("buildVersion", LevelSession::kBuildVersion);
        level.set("name", o.level.name);
        level.set("comments", o.level.comments);
        level.set("data", o.level.data);
        level.set("kind", o.level.kind);
        level.set("force_character", o.level.forceCharacter);
        level.set("playable_character", o.level.playableCharacter);
        o.channel->send(level);
        o.state = SendState::Sending;
        o.deadline = NetLoop::now() + kSenderResultTimeout;
        if (o.callback) o.callback(SendState::Sending, "Sending to " + who + "…");
        return;
    }
    if (m.type == "result" && o.state == SendState::Sending) {
        o.channel->close();
        if (m.getBool("ok")) {
            finishOutgoing(id, SendState::Done, who + " got “" + o.level.name + "”.");
        } else {
            finishOutgoing(id, SendState::Failed, who + " could not use the level: " + m.get("error"));
        }
        return;
    }
}

void LevelTransfer::onOutgoingClosed(int id, const std::string& error) {
    auto it = _outgoing.find(id);
    if (it == _outgoing.end()) return;
    Outgoing& o = *it->second;
    std::string text;
    if (o.state == SendState::Connecting) {
        text = "Could not reach " + o.target.name + ". Are you on the same Wi-Fi?";
    } else if (o.state == SendState::Waiting) {
        text = o.target.name + " did not answer.";
    } else {
        text = "The connection to " + o.target.name + " was lost.";
    }
    if (!error.empty()) log("net: send to %s: %s", o.target.name.c_str(), error.c_str());
    finishOutgoing(id, SendState::Failed, text);
}

void LevelTransfer::finishOutgoing(int id, SendState state, const std::string& message) {
    auto it = _outgoing.find(id);
    if (it == _outgoing.end()) return;
    SendCallback cb = it->second->callback;
    std::shared_ptr<Channel> keep = it->second->channel;
    keep->clearHandler();
    _outgoing.erase(it);
    NetLoop::instance()->post([keep]() {});
    if (cb) cb(state, message);
}

// ---- timers ----------------------------------------------------------------------------------

void LevelTransfer::tick(float) {
    const double now = NetLoop::now();
    std::vector<int> expired;
    for (auto& kv : _incoming) {
        Incoming& in = *kv.second;
        if (in.state == Incoming::State::Pending && canInterruptNow()) showOffer(in);
        if (in.state == Incoming::State::Asking && _offerWindow && !_offerWindow->getParent()) {
            // The scene under the popup went away: show it again when possible.
            _offerWindow->release();
            _offerWindow = nullptr;
            in.state = Incoming::State::Pending;
        }
        if (now > in.deadline) expired.push_back(kv.first);
    }
    for (int id : expired) {
        auto it = _incoming.find(id);
        if (it == _incoming.end()) continue;
        Incoming& in = *it->second;
        if (in.state == Incoming::State::Pending || in.state == Incoming::State::Asking) {
            in.channel->send(Message("answer").set("accept", false).set("reason", "timeout"));
            in.channel->close();
            if (_offerWindow) _offerWindow->dismissWindow(true);
        } else {
            in.channel->abort();
        }
        if (id == _askingId) _askingId = 0;
        finishIncoming(id);
    }
    std::vector<int> late;
    for (auto& kv : _outgoing) {
        if (now > kv.second->deadline) late.push_back(kv.first);
    }
    for (int id : late) {
        auto it = _outgoing.find(id);
        if (it == _outgoing.end()) continue;
        Outgoing& o = *it->second;
        o.channel->abort();
        const std::string who = o.target.name;
        finishOutgoing(id, SendState::Failed,
                       o.state == SendState::Connecting ? "Could not reach " + who + "." : who + " did not answer.");
    }
}

}  // namespace net
