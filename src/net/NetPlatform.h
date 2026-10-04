#pragma once
// NET (PC addition): thin BSD-socket layer shared by the LAN features (src/net/). Winsock on
// Windows, POSIX sockets on Android. Not part of the 1:1 reconstruction.
//
// Addresses are IPv4 in HOST byte order (uint32_t) everywhere in src/net/; only this file and the
// .cpp files that call the socket API convert with htonl/ntohl.

#include <cstdint>
#include <string>
#include <vector>

namespace net {

#ifdef _WIN32
using SocketHandle = std::uintptr_t;   // SOCKET
#else
using SocketHandle = int;
#endif
extern const SocketHandle kInvalidSocket;

// WSAStartup on Windows (once); no-op elsewhere. Returns false when sockets are unavailable.
bool startup();
void closeSocket(SocketHandle s);
bool setNonBlocking(SocketHandle s);
// The last socket call failed only because it would block / is in progress.
bool lastErrorWouldBlock();
int lastError();
std::string lastErrorText();

// One IPv4 network interface that is up (loopback included, flagged).
struct Interface {
    std::string name;
    uint32_t address = 0;   // host order
    uint32_t netmask = 0;   // host order (0 when unknown)
    bool loopback = false;
    uint32_t broadcast() const { return netmask ? ((address & netmask) | ~netmask) : 0xffffffffu; }
};
std::vector<Interface> ipv4Interfaces();
// The address other devices most likely reach us at: the first private (10/8, 172.16/12,
// 192.168/16) non-loopback interface, else any non-loopback, non-link-local one, else 127.0.0.1.
uint32_t primaryAddress();

// Computer name (Windows) or device model (Android), e.g. "MY-PC" / "Pixel 7".
std::string deviceName();

std::string ipToString(uint32_t hostOrder);
bool parseIPv4(const std::string& text, uint32_t* hostOrder);

// Android: WifiManager.MulticastLock (AppActivity.setMulticastLock), so the Wi-Fi driver hands
// broadcast/multicast datagrams to the app. No-op elsewhere.
void setMulticastLock(bool held);

}  // namespace net
