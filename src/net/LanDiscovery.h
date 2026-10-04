#pragma once
// NET (PC addition): finds other OpenWheels instances on the local network. Generic: it only
// announces "who am I and which TCP port do I listen on" plus a list of services; the level
// transfer and a later ghost-race mode both build their peer lists from it.
//
// Wire format: one UDP datagram (<= 1200 bytes) of UTF-8 text lines, "key=value", the first line
// being the magic "OWLAN":
//   OWLAN
//   t=beacon | query        beacon: an announcement; query: "everyone please announce now"
//   v=1                     discovery protocol version (peers with another version are ignored)
//   app=OpenWheels
//   id=<16 hex digits>      random per process (two instances on one PC differ)
//   name=<player name>      editable, UserDefault "net_player_name"
//   dev=<device name>       computer name / phone model
//   plat=pc | phone         for the device icon
//   port=<tcp port>         the instance's Listener port
//   svc=levels[,ghost...]   services offered
// Values are single-line (newlines are replaced by spaces).
//
// Sockets (all on the NetLoop worker):
//   * the shared socket: UDP kDiscoveryPort (47810) with SO_REUSEADDR, joined to the multicast
//     group 239.255.77.81 on every interface; receives beacons and queries of everybody, also of
//     other instances on the same machine;
//   * the send socket: an ephemeral UDP port. Beacons go out every 1.5 s to the multicast group
//     (once per interface, IP_MULTICAST_IF), to every interface's directed broadcast, to
//     255.255.255.255 and to 127.0.0.1:47810. Queries go out the same way when browsing starts;
//     peers answer a query with a unicast beacon to the send socket.
// Interfaces are re-enumerated every 10 s (Wi-Fi changes). Peers vanish 6 s after their last
// beacon.

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace net {

constexpr uint16_t kDiscoveryPort = 47810;
constexpr const char* kMulticastGroup = "239.255.77.81";
constexpr int kDiscoveryVersion = 1;

struct Peer {
    std::string id;           // instance id
    std::string name;         // player name
    std::string device;       // device name
    std::string platform;     // "pc" | "phone"
    uint32_t address = 0;     // host order, where the beacon came from
    uint16_t port = 0;        // TCP port
    std::vector<std::string> services;
    double lastSeen = 0.0;    // NetLoop::now()
    bool offers(const std::string& service) const;
};

class LanDiscovery {
public:
    using PeersCallback = std::function<void(const std::vector<Peer>& peers)>;

    static LanDiscovery* getInstance();   // cocos thread

    // Starts announcing (and answering queries). Calling again updates the announced info.
    void announce(const std::string& name, const std::string& device, uint16_t tcpPort,
                  const std::vector<std::string>& services);
    void stop();
    const std::string& instanceId() const;

    // Browsing: while at least one observer exists, beacons are collected and `callback` runs on
    // the cocos thread whenever the peer list changes (and once right away). On Android this
    // holds a Wi-Fi MulticastLock. The returned id removes the observer.
    int addObserver(PeersCallback callback);
    void removeObserver(int observerId);
    // Current peers (cocos thread copy), sorted by name.
    const std::vector<Peer>& peers() const { return _peers; }
    // Sends a query so peers answer at once (done automatically when browsing starts).
    void refresh();

private:
    LanDiscovery();
    class Worker;
    void peersChanged(std::vector<Peer> peers);   // cocos thread

    std::string _id;
    std::shared_ptr<Worker> _worker;
    std::map<int, PeersCallback> _observers;
    int _nextObserver = 1;
    std::vector<Peer> _peers;
};

}  // namespace net
