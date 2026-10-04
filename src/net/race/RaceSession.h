#pragma once
// NET (PC addition): ghost race - local races between players on the same Wi-Fi (PC <-> PC,
// PC <-> Android). See docs/RACE.md.
//
// Everyone plays the same level in their own world; the other racers appear as ghosts (GhostView.h)
// built from snapshots of their rider and vehicle art (GhostCapture.h). Physics are never shared.
//
// Topology: a star. The host keeps one net::Channel per guest (the TCP listener and discovery of
// the level transfer are reused: a hello with purpose "race" is handed over from LevelTransfer)
// and relays every snapshot and status to the other guests.
//
//   host                                     guest
//   hello {app, proto, purpose=race, ...} -->
//                                       <--  hello
//   invite {level, kind, host, players}   -->  HWWindow "<host> invites you to race" Join / Decline
//                                       <--  answer {accept, reason}
//   welcome {you}, lobby {...}            -->  lobby panel
//                                       <--  pick {character}, ready {ready}
//   load {level fields, xml}              -->  the level starts frozen
//                                       <--  loaded
//   go {delay ms}                         -->  3-2-1 countdown, then everyone rides
//   s {d, f}  (snapshots, both ways; the host adds f = sender id when relaying)
//   st {status, time}  (racing / ejected / dead / finished / gaveup)  -> host -> lobby {...} to all
//   results {...}, lobby {phase=lobby}, end {reason}, left {id, name}     bye (guest leaves)

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cocos2d.h"
#include "net/Channel.h"
#include "net/LanDiscovery.h"
#include "net/race/GhostCapture.h"
#include "net/race/GhostView.h"
#include "net/race/RaceHooks.h"

class Gameplay;
class HWWindow;

namespace race {

enum class Phase { Idle, Lobby, Loading, Countdown, Racing, Results };

enum class Status { Lobby, Loading, Loaded, Racing, Ejected, Dead, Finished, GaveUp, Left };
const char* statusName(Status s);
Status statusFromName(const std::string& s);
bool statusIsFinal(Status s);   // Finished / GaveUp / Left

struct RaceLevel {
    std::string kind;             // "campaign" | "mobile" (your levels) | "online"
    std::string name;
    int chapter = 0, level = 0;   // campaign
    std::string xml;              // mobile / online (online: already converted)
    bool forced = false;          // the level picks the rider
    int forcedCharacter = 1;
    bool valid() const { return kind == "campaign" || !xml.empty(); }
    std::string kindLabel() const;
};

struct RacePlayer {
    int id = 0;
    std::string name;
    std::string platform;   // "pc" | "phone"
    int slot = 0;           // colour
    int character = 2;
    bool ready = false;
    Status status = Status::Lobby;
    int timeMs = -1;        // finish time
    int place = 0;          // results
};

struct InviteInfo {
    enum State { Connecting, Waiting, Declined, Failed, Joined };
    State state = Connecting;
    std::string text;
};

class RaceHud;
class LobbyPanel;
class ResultsPanel;

class RaceSession {
public:
    static RaceSession* get();

    // ---- entry points ----
    // Main menu "Race": the open race's lobby, or a new race on a campaign level.
    void openMenu();
    // "Race on this level" (your levels / online browser): hosts a race on `level`.
    void hostLevel(const RaceLevel& level);
    // LevelTransfer hands over incoming connections whose hello says purpose=race.
    void acceptIncoming(const std::shared_ptr<net::Channel>& channel, const net::Message& hello);
    // PC test hook (--race-test): "host:campaign:<chapter>:<level>", "host:online:<file>",
    // "host:level:<file>" or "join"; players = how many racers the host waits for.
    void setAutoTest(const std::string& spec, int players, int character);

    // ---- host ----
    void invite(const net::Peer& peer);
    void inviteTarget(uint32_t address, uint16_t port, const std::string& name);
    void setLevel(const RaceLevel& level);
    bool canStart(std::string* why) const;
    void start();
    void rematch();
    void backToLobby();
    void endRace();       // results now (DNF for those still riding)

    // ---- everyone ----
    void pickCharacter(int characterId);
    void setReady(bool ready);
    void giveUp();
    bool aloneInOwnLobby() const;  // hosting a lobby nobody has joined yet
    void leave();         // guest: leave the race; host: close it for everyone

    // ---- state for the UI ----
    Phase phase() const { return _phase; }
    bool isHost() const { return _host; }
    bool active() const { return _phase != Phase::Idle; }
    int localId() const { return _localId; }
    const RaceLevel& level() const { return _level; }
    const std::vector<RacePlayer>& players() const { return _players; }
    const RacePlayer* player(int id) const;
    const RacePlayer* localPlayer() const { return player(_localId); }
    const std::map<std::string, InviteInfo>& invites() const { return _invites; }
    std::string hostName() const { return _hostName; }
    double raceClock() const;              // seconds since GO (0 before)
    double countdownLeft() const;          // seconds to GO while counting down
    float progressOf(int playerId) const;  // 0..1, -1 = unknown
    std::string statusLine() const;        // lobby status text
    static cocos2d::Color3B slotColor(int slot);
    static std::string characterName(int characterId);
    static std::vector<int> characterIds();

    // ---- game hooks ----
    bool suppressVictoryMenu() const;      // Gameplay::handleLevelComplete while racing
    void tick(float dt);                   // every frame (installed on first use)

    // UI notifications
    void panelClosed(LobbyPanel* panel);
    void resultsClosed(ResultsPanel* panel);

private:
    RaceSession();
    struct Conn {
        int playerId = 0;
        std::shared_ptr<net::Channel> channel;
    };
    struct PendingInvite {
        std::string peerId;
        std::shared_ptr<net::Channel> channel;
        std::string name;
        double deadline = 0.0;
        bool answered = false;
    };
    struct IncomingInvite {
        std::shared_ptr<net::Channel> channel;
        std::string hostName, hostDevice, levelName, kind;
        int players = 0;
        double deadline = 0.0;
        bool shown = false;
        bool gotInvite = false;
    };

    void ensureTicking();
    void reset();
    void setPhase(Phase p);
    void changed();
    RacePlayer* mutablePlayer(int id);
    int freeSlot() const;
    net::Message helloMessage() const;
    net::Message lobbyMessage() const;
    void broadcast(const net::Message& m, int exceptId = 0);
    void sendToHost(const net::Message& m);
    void broadcastLobby();
    void hostAddPlayer(const std::shared_ptr<net::Channel>& channel, const std::string& name, const std::string& platform);
    void hostMessage(int playerId, const net::Message& m);
    void hostClosed(int playerId, const std::string& error);
    void startInvite(const std::string& key, uint32_t address, uint16_t port, const std::string& name);
    void inviteMessage(const std::string& peerId, const net::Message& m);
    void inviteClosed(const std::string& peerId);
    void guestMessage(const net::Message& m);
    void guestClosed(const std::string& error);
    void incomingMessage(int id, const net::Message& m);
    void incomingClosed(int id);
    void showIncoming();
    void answerIncoming(bool accept);
    void readLobby(const net::Message& m);
    void playerLeft(int id, const std::string& name);
    void localStatus(Status s, int timeMs = -1);
    void hostSetStatus(int playerId, Status s, int timeMs);
    void checkResults();
    void computePlaces();

    // the level
    void startLocalLevel();
    void leaveGameplay();
    void goToLobbyScene();
    void onLevelComplete();
    void onCharacterEvent(const std::string& name);
    void integrateScene();
    void captureAndSend(Gameplay* g);
    void drawGhosts();
    void receiveSnapshot(int fromId, const std::string& data);
    void autoTestTick();

    bool _ticking = false;
    Phase _phase = Phase::Idle;
    bool _host = false;
    int _localId = 0;
    int _nextPlayerId = 2;
    RaceLevel _level;
    std::vector<RacePlayer> _players;
    std::map<int, Conn> _conns;                         // host: by player id
    std::map<std::string, PendingInvite> _pending;      // host: by peer id
    std::map<std::string, InviteInfo> _invites;         // host: invite states for the UI
    std::shared_ptr<net::Channel> _hostChannel;         // guest
    std::string _hostName;
    std::map<int, IncomingInvite> _incoming;            // guest: invites not answered yet
    int _nextIncoming = 1;
    int _askingIncoming = 0;
    HWWindow* _inviteWindow = nullptr;

    // race state
    int _raceSerial = 0;
    double _goAt = 0.0;             // NetLoop::now() of GO
    double _loadStarted = 0.0;
    bool _sentLoaded = false;
    std::map<int, bool> _loaded;    // host
    Status _localStatus = Status::Lobby;

    // scene integration
    cocos2d::RefPtr<cocos2d::Node> _hud;          // RaceHud in the current Gameplay
    cocos2d::RefPtr<cocos2d::Node> _ghostLayer;   // GhostLayer in the current Session
    Gameplay* _gameplay = nullptr;                // compared only (never dereferenced unless found in the scene)
    int _levelSeenFrames = 0;
    bool _frozen = false;
    uint32_t _run = 0;
    GhostCapture _capture;
    double _lastCapture = -1.0;
    cocos2d::Vec2 _startFocus;
    bool _haveStart = false;
    cocos2d::Vec2 _finishPos;
    bool _haveFinish = false;
    cocos2d::Vec2 _localFocus;
    std::map<int, GhostTrack> _tracks;            // remote players
    std::map<int, double> _fadeOut;               // player id -> time the fade started (left)

    // UI
    cocos2d::RefPtr<cocos2d::Node> _lobbyPanel;
    cocos2d::RefPtr<cocos2d::Node> _resultsPanel;
    bool _resultsShown = false;
    int _lobbyObserver = 0;

    // statistics
    size_t _txBytes = 0, _txSnaps = 0;
    double _statsSince = 0.0;

    // test hook
    std::string _autoSpec;
    int _autoPlayers = 2;
    int _autoCharacter = 0;
    bool _autoHosted = false;
    double _autoReadyAt = 0.0;
};


}  // namespace race
