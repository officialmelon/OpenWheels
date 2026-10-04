#pragma once
// NET (PC addition): "Send to nearby" - level transfer between OpenWheels instances on the
// local network (PC <-> PC, Android <-> Android, PC <-> Android). No accounts, no servers.
//
// Every running game listens on TCP 47811 (the next free one up to 47826 when several instances
// share a machine) and announces itself through LanDiscovery with the service "levels". A sender
// picks a peer (or types a receive code) and opens a net::Channel:
//
//   sender                                   receiver
//   hello {app, proto, purpose=level,  -->
//          name, dev, id}
//                                      <--  hello {app, proto, name, dev, id}
//   offer {name, comments, kind,       -->  checks + rate limit; HWWindow
//          size, character, force}          "<name> wants to send you "<level>"" Accept / Decline
//                                      <--  answer {accept=0|1, reason}
//   level {name, comments, data, kind, -->  (accept=1 only) size cap, XML must parse (flash
//          playable_character,              levels are converted first), saved into "your
//          force_character, buildVersion}   levels", Received tab (chapter 5001, the import chapter)
//                                      <--  result {ok=0|1, error, saved}
// The level message carries the same fields as the editor's Share (.happywheels plist:
// buildVersion, name, comments, data, force_character, playable_character) plus "kind":
// "mobile" = mobile level XML, "flash" = a downloaded browser level (online/), which the receiver
// converts with FlashLevelConverter like the online browser does.
// Every frame has a length and a CRC-32 (Message.h). A level is at most 4 MiB.
//
// Safety: offers are never accepted automatically. One offer is on screen at a time (others
// are answered "busy"); a sender address may offer once per 3 s and must wait 10 s after a
// decline; at most 10 offers a minute are considered. While a level is being PLAYED (gameplay
// running, not paused) the popup waits until the game is paused or left. Offers time out after
// 60 s.
//
// Receive code (fallback when broadcasts are blocked): the receiver's IPv4 address and port
// offset (port - 47811) packed with a 4-bit check into 8 Crockford base-32 characters, shown as
// "XXXX-XXXX". The sender may also type a plain "a.b.c.d" or "a.b.c.d:port".

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "net/Channel.h"
#include "net/LanDiscovery.h"

class HWWindow;
class LevelMO;

namespace net {

constexpr uint16_t kTransferPort = 47811;
constexpr int kTransferPortCount = 16;
constexpr size_t kMaxLevelBytes = 4u * 1024u * 1024u;
constexpr int kTransferProtocol = 1;
constexpr const char* kLevelsService = "levels";

struct LevelPackage {
    std::string name;
    std::string comments;
    std::string data;                 // level XML
    int playableCharacter = 1;
    bool forceCharacter = false;
    std::string kind = "mobile";      // "mobile" | "flash"

    static LevelPackage fromLevel(LevelMO* level);
};

struct SendTarget {
    uint32_t address = 0;   // host order
    uint16_t port = 0;
    std::string name;       // for messages ("" = address)
};

class LevelTransfer {
public:
    static LevelTransfer* getInstance();

    // Opens the listener and starts announcing. Cocos thread; call once at start-up.
    void start();
    void shutdown();
    bool isListening() const { return _listener != nullptr; }
    uint16_t port() const;

    // Player name shown to others: UserDefault "net_player_name", default deviceName().
    std::string playerName() const;
    void setPlayerName(const std::string& name);
    static std::string sanitizeName(const std::string& name);

    // ---- receive code ----
    std::string receiveCode() const;                     // "" when not listening
    std::string receiveAddressText() const;              // "192.168.1.20:47811"
    static std::string encodeCode(uint32_t address, uint16_t port);
    // A code, "a.b.c.d" or "a.b.c.d:port". False when it cannot be read.
    static bool parseTarget(const std::string& text, uint32_t* address, uint16_t* port);

    // ---- sending ----
    enum class SendState { Connecting, Waiting, Sending, Done, Declined, Failed };
    using SendCallback = std::function<void(SendState state, const std::string& message)>;
    // Returns a send id. callback runs on the cocos thread for every state change; Done,
    // Declined and Failed are final.
    int send(const SendTarget& target, const LevelPackage& level, SendCallback callback);
    void cancelSend(int sendId);

    // Port: the receive side's popups are only shown when this returns true (no transition, no
    // unpaused gameplay).
    static bool canInterruptNow();

private:
    LevelTransfer();
    struct Outgoing;
    struct Incoming;
    friend class TransferAlertDelegate;

    void accept(const std::shared_ptr<Channel>& channel);
    void onIncomingMessage(int id, const Message& message);
    void onIncomingClosed(int id, const std::string& error);
    void handleOffer(Incoming& in, const Message& offer);
    void handleLevel(Incoming& in, const Message& level);
    void showOffer(Incoming& in);
    void answerOffer(int id, bool accept);
    void finishIncoming(int id);
    void tick(float dt);
    void showReceived(LevelMO* level, const std::string& from);
    void showAlert(const std::string& title, const std::string& message);

    void onOutgoingMessage(int id, const Message& message);
    void onOutgoingClosed(int id, const std::string& error);
    void finishOutgoing(int id, SendState state, const std::string& message);

    std::shared_ptr<Listener> _listener;
    bool _started = false;
    std::map<int, std::unique_ptr<Incoming>> _incoming;
    std::map<int, std::unique_ptr<Outgoing>> _outgoing;
    int _nextId = 1;
    int _askingId = 0;                         // incoming offer on screen / waiting for its data
    HWWindow* _offerWindow = nullptr;          // retained while on screen
    std::map<uint32_t, double> _lastOfferFrom; // rate limits, NetLoop::now()
    std::map<uint32_t, double> _declinedUntil;
    std::vector<double> _recentOffers;
};

}  // namespace net
