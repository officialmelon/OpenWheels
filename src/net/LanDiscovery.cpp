// NET (PC addition): LAN discovery, see LanDiscovery.h.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "net/LanDiscovery.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <random>
#include <set>
#include <sstream>

#include "net/NetLoop.h"
#include "net/NetPlatform.h"

namespace net {

namespace {

#ifdef _WIN32
using SockLen = int;
#else
using SockLen = socklen_t;
#endif

constexpr double kBeaconInterval = 1.5;
constexpr double kPeerTimeout = 6.0;
constexpr double kInterfaceRefresh = 10.0;
constexpr size_t kMaxDatagram = 1200;

sockaddr_in makeAddress(uint32_t ip, uint16_t port) {
    sockaddr_in a;
    std::memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(ip);
    a.sin_port = htons(port);
    return a;
}

std::string oneLine(std::string s) {
    for (char& c : s) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    return s.size() > 200 ? s.substr(0, 200) : s;
}

std::string randomId() {
    std::random_device rd;
    std::mt19937_64 gen((static_cast<uint64_t>(rd()) << 32) ^ rd() ^
                        static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(gen()));
    return buf;
}

}  // namespace

bool Peer::offers(const std::string& service) const {
    return std::find(services.begin(), services.end(), service) != services.end();
}

// ---- worker ------------------------------------------------------------------------------------

class LanDiscovery::Worker : public Pollable, public std::enable_shared_from_this<Worker> {
public:
    explicit Worker(std::string id) : _id(std::move(id)) {
        parseIPv4(kMulticastGroup, &_group);
    }
    ~Worker() override {
        closeSocket(_shared);
        closeSocket(_sender);
    }

    void setInfo(const std::string& name, const std::string& device, uint16_t port,
                 const std::vector<std::string>& services) {
        std::lock_guard<std::mutex> lock(_mutex);
        _name = oneLine(name);
        _device = oneLine(device);
        _port = port;
        _services = services;
        _announcing = true;
        _nextBeacon = 0.0;   // announce the change right away
    }
    void stopAnnouncing() {
        std::lock_guard<std::mutex> lock(_mutex);
        _announcing = false;
    }
    void setBrowsing(bool browsing) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (browsing && !_browsing) _queryPending = true;
        if (!browsing) _peers.clear();
        _browsing = browsing;
    }
    void query() {
        std::lock_guard<std::mutex> lock(_mutex);
        _queryPending = true;
    }
    void finish() {
        std::lock_guard<std::mutex> lock(_mutex);
        _finished = true;
    }

    void collect(std::vector<SocketHandle>& read, std::vector<SocketHandle>&) override {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_shared != kInvalidSocket) read.push_back(_shared);
        if (_sender != kInvalidSocket) read.push_back(_sender);
    }
    bool finished() const override {
        std::lock_guard<std::mutex> lock(_mutex);
        return _finished;
    }

    void process(double now) override {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_finished) return;
        if (now >= _nextInterfaceRefresh) {
            _interfaces = ipv4Interfaces();
            _nextInterfaceRefresh = now + kInterfaceRefresh;
            openSockets();
            joinGroups();
        }
        receive(_shared, now);
        receive(_sender, now);
        if (_announcing && now >= _nextBeacon) {
            broadcast(packet("beacon"));
            _nextBeacon = now + kBeaconInterval;
        }
        if (_queryPending && _browsing) {
            _queryPending = false;
            broadcast(packet("query"));
        }
        if (_browsing) {
            bool changed = _changed;
            _changed = false;
            for (auto it = _peers.begin(); it != _peers.end();) {
                if (now - it->second.lastSeen > kPeerTimeout) {
                    it = _peers.erase(it);
                    changed = true;
                } else {
                    ++it;
                }
            }
            if (changed) publish();
        }
    }

private:
    void openSockets() {
        if (_shared == kInvalidSocket) {
            SocketHandle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s != kInvalidSocket) {
                int on = 1;
                setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof(on));
                setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof(on));
                sockaddr_in a = makeAddress(0, kDiscoveryPort);
                if (bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0) {
                    int loop = 1;   // DWORD on Windows, int accepted on Linux
                    setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loop), sizeof(loop));
                    setNonBlocking(s);
                    _shared = s;
                    _joined.clear();
                } else {
                    closeSocket(s);
                }
            }
        }
        if (_sender == kInvalidSocket) {
            SocketHandle s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s != kInvalidSocket) {
                int on = 1;
                setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof(on));
                sockaddr_in a = makeAddress(0, 0);
                bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a));
                int loop = 1;   // DWORD on Windows, int accepted on Linux
                setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loop), sizeof(loop));
                int ttl = 1;
                setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));
                setNonBlocking(s);
                _sender = s;
            }
        }
    }

    void joinGroups() {
        if (_shared == kInvalidSocket) return;
        std::vector<uint32_t> targets;
        targets.push_back(0);   // INADDR_ANY: the default interface
        for (const Interface& i : _interfaces) targets.push_back(i.address);
        for (uint32_t itf : targets) {
            if (_joined.count(itf)) continue;
            ip_mreq m;
            std::memset(&m, 0, sizeof(m));
            m.imr_multiaddr.s_addr = htonl(_group);
            m.imr_interface.s_addr = htonl(itf);
            setsockopt(_shared, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&m), sizeof(m));
            _joined.insert(itf);   // failures (already joined, no multicast) are not retried
        }
    }

    std::string packet(const char* type) const {
        std::ostringstream o;
        o << "OWLAN\n";
        o << "t=" << type << "\n";
        o << "v=" << kDiscoveryVersion << "\n";
        o << "app=OpenWheels\n";
        o << "id=" << _id << "\n";
        if (_announcing) {
            o << "name=" << _name << "\n";
            o << "dev=" << _device << "\n";
#if defined(__ANDROID__)
            o << "plat=phone\n";
#else
            o << "plat=pc\n";
#endif
            o << "port=" << _port << "\n";
            o << "svc=";
            for (size_t i = 0; i < _services.size(); ++i) o << (i ? "," : "") << _services[i];
            o << "\n";
        }
        std::string s = o.str();
        return s.size() > kMaxDatagram ? s.substr(0, kMaxDatagram) : s;
    }

    void sendTo(const std::string& data, uint32_t ip, uint16_t port) {
        if (_sender == kInvalidSocket) return;
        sockaddr_in a = makeAddress(ip, port);
        sendto(_sender, data.data(), static_cast<int>(data.size()), 0, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    }

    void broadcast(const std::string& data) {
        if (_sender == kInvalidSocket) return;
        // multicast once per interface
        for (const Interface& i : _interfaces) {
            in_addr itf;
            itf.s_addr = htonl(i.address);
            setsockopt(_sender, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&itf), sizeof(itf));
            sendTo(data, _group, kDiscoveryPort);
        }
        // directed broadcasts, the limited broadcast, loopback
        std::set<uint32_t> sent;
        for (const Interface& i : _interfaces) {
            if (i.loopback || !i.netmask) continue;
            const uint32_t b = i.broadcast();
            if (sent.insert(b).second) sendTo(data, b, kDiscoveryPort);
        }
        sendTo(data, 0xffffffffu, kDiscoveryPort);
        sendTo(data, 0x7f000001u, kDiscoveryPort);
    }

    void receive(SocketHandle s, double now) {
        if (s == kInvalidSocket) return;
        char buf[kMaxDatagram + 1];
        for (int i = 0; i < 64; ++i) {
            sockaddr_in from;
            SockLen len = sizeof(from);
            const int n = static_cast<int>(recvfrom(s, buf, kMaxDatagram, 0, reinterpret_cast<sockaddr*>(&from), &len));
            if (n <= 0) return;
            handle(std::string(buf, static_cast<size_t>(n)), ntohl(from.sin_addr.s_addr), ntohs(from.sin_port), now);
        }
    }

    int score(uint32_t address) const {
        if ((address >> 24) == 127) return 1;
        for (const Interface& i : _interfaces) {
            if (!i.loopback && i.netmask && (i.address & i.netmask) == (address & i.netmask)) return 3;
        }
        return 2;
    }

    void handle(const std::string& data, uint32_t fromIp, uint16_t fromPort, double now) {
        std::istringstream in(data);
        std::string line;
        if (!std::getline(in, line) || line != "OWLAN") return;
        std::map<std::string, std::string> kv;
        while (std::getline(in, line)) {
            const size_t eq = line.find('=');
            if (eq != std::string::npos) kv[line.substr(0, eq)] = line.substr(eq + 1);
        }
        if (kv["app"] != "OpenWheels" || kv["v"] != std::to_string(kDiscoveryVersion)) return;
        const std::string& id = kv["id"];
        if (id.empty() || id == _id) return;
        const std::string& type = kv["t"];
        if (type == "query") {
            // Answer with a unicast beacon (at most every 0.25 s per asker).
            const uint64_t key = (static_cast<uint64_t>(fromIp) << 16) | fromPort;
            if (_announcing && now - _lastAnswer[key] > 0.25) {
                _lastAnswer[key] = now;
                sendTo(packet("beacon"), fromIp, fromPort);
            }
            return;
        }
        if (type != "beacon" || !_browsing) return;
        const long port = std::strtol(kv["port"].c_str(), nullptr, 10);
        if (port <= 0 || port > 65535) return;
        Peer& p = _peers[id];
        const bool isNew = p.id.empty();
        Peer before = p;
        p.id = id;
        p.name = kv["name"].empty() ? kv["dev"] : kv["name"];
        p.device = kv["dev"];
        p.platform = kv["plat"] == "phone" ? "phone" : "pc";
        p.port = static_cast<uint16_t>(port);
        p.services.clear();
        std::istringstream svc(kv["svc"]);
        std::string s;
        while (std::getline(svc, s, ',')) {
            if (!s.empty()) p.services.push_back(s);
        }
        if (isNew || score(fromIp) >= score(p.address)) p.address = fromIp;
        p.lastSeen = now;
        if (isNew || before.name != p.name || before.device != p.device || before.port != p.port ||
            before.address != p.address || before.services != p.services) {
            _changed = true;
        }
    }

    void publish() {
        std::vector<Peer> list;
        for (auto& kv : _peers) list.push_back(kv.second);
        std::sort(list.begin(), list.end(), [](const Peer& a, const Peer& b) {
            return a.name == b.name ? a.id < b.id : a.name < b.name;
        });
        NetLoop::instance()->post([list]() { LanDiscovery::getInstance()->peersChanged(list); });
    }

    mutable std::mutex _mutex;
    std::string _id;
    uint32_t _group = 0;
    SocketHandle _shared = kInvalidSocket;
    SocketHandle _sender = kInvalidSocket;
    std::vector<Interface> _interfaces;
    std::set<uint32_t> _joined;
    double _nextInterfaceRefresh = 0.0;
    double _nextBeacon = 0.0;
    bool _announcing = false;
    bool _browsing = false;
    bool _queryPending = false;
    bool _changed = false;
    bool _finished = false;
    std::string _name, _device;
    uint16_t _port = 0;
    std::vector<std::string> _services;
    std::map<std::string, Peer> _peers;
    std::map<uint64_t, double> _lastAnswer;
};

// ---- LanDiscovery ------------------------------------------------------------------------------

LanDiscovery* LanDiscovery::getInstance() {
    static LanDiscovery* instance = new LanDiscovery();
    return instance;
}

LanDiscovery::LanDiscovery() {
    _id = randomId();
    _worker = std::make_shared<Worker>(_id);
    NetLoop::instance()->add(_worker);
}

const std::string& LanDiscovery::instanceId() const {
    return _id;
}

void LanDiscovery::announce(const std::string& name, const std::string& device, uint16_t tcpPort,
                            const std::vector<std::string>& services) {
    _worker->setInfo(name, device, tcpPort, services);
}

void LanDiscovery::stop() {
    _worker->stopAnnouncing();
}

int LanDiscovery::addObserver(PeersCallback callback) {
    const int id = _nextObserver++;
    const bool first = _observers.empty();
    _observers[id] = callback;
    if (first) {
        _peers.clear();
        setMulticastLock(true);
        _worker->setBrowsing(true);
    }
    if (callback) callback(_peers);
    return id;
}

void LanDiscovery::removeObserver(int observerId) {
    if (!_observers.erase(observerId)) return;
    if (_observers.empty()) {
        _worker->setBrowsing(false);
        setMulticastLock(false);
        _peers.clear();
    }
}

void LanDiscovery::refresh() {
    _worker->query();
}

void LanDiscovery::peersChanged(std::vector<Peer> peers) {
    if (_observers.empty()) return;
    _peers = std::move(peers);
    // Copy: an observer may remove itself.
    std::map<int, PeersCallback> observers = _observers;
    for (auto& kv : observers) {
        if (_observers.count(kv.first) && kv.second) kv.second(_peers);
    }
}

}  // namespace net
