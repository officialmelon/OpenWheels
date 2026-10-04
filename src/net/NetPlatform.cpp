// NET (PC addition): socket helpers, see NetPlatform.h.
#include "net/NetPlatform.h"

#include <cstdio>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#include "platform/android/jni/JniHelper.h"
#endif

namespace net {

#ifdef _WIN32
const SocketHandle kInvalidSocket = static_cast<SocketHandle>(INVALID_SOCKET);
#else
const SocketHandle kInvalidSocket = -1;
#endif

bool startup() {
#ifdef _WIN32
    static std::once_flag once;
    static bool ok = false;
    std::call_once(once, []() {
        WSADATA data;
        ok = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    });
    return ok;
#else
    return true;
#endif
}

void closeSocket(SocketHandle s) {
    if (s == kInvalidSocket) return;
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(s));
#else
    close(s);
#endif
}

bool setNonBlocking(SocketHandle s) {
#ifdef _WIN32
    u_long on = 1;
    return ioctlsocket(static_cast<SOCKET>(s), FIONBIO, &on) == 0;
#else
    int flags = fcntl(s, F_GETFL, 0);
    return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

int lastError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool lastErrorWouldBlock() {
    const int e = lastError();
#ifdef _WIN32
    return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY;
#else
    return e == EWOULDBLOCK || e == EAGAIN || e == EINPROGRESS || e == EALREADY;
#endif
}

std::string lastErrorText() {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "socket error %d", lastError());
    return buf;
}

std::vector<Interface> ipv4Interfaces() {
    std::vector<Interface> result;
    if (!startup()) return result;
#ifdef _WIN32
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return result;
    INTERFACE_INFO info[64];
    DWORD bytes = 0;
    if (WSAIoctl(s, SIO_GET_INTERFACE_LIST, nullptr, 0, info, sizeof(info), &bytes, nullptr, nullptr) == 0) {
        const int count = static_cast<int>(bytes / sizeof(INTERFACE_INFO));
        for (int i = 0; i < count; ++i) {
            if (!(info[i].iiFlags & IFF_UP) || info[i].iiAddress.Address.sa_family != AF_INET) continue;
            Interface itf;
            itf.address = ntohl(info[i].iiAddress.AddressIn.sin_addr.s_addr);
            itf.netmask = ntohl(info[i].iiNetmask.AddressIn.sin_addr.s_addr);
            itf.loopback = (info[i].iiFlags & IFF_LOOPBACK) != 0 || (itf.address >> 24) == 127;
            itf.name = "if" + std::to_string(i);
            if (itf.address != 0) result.push_back(itf);
        }
    }
    closesocket(s);
#else
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return result;
    // SIOCGIFCONF (getifaddrs needs API 24; the game supports API 21).
    char buf[64 * sizeof(struct ifreq)];
    struct ifconf conf;
    conf.ifc_len = sizeof(buf);
    conf.ifc_buf = buf;
    if (ioctl(s, SIOCGIFCONF, &conf) == 0) {
        const int count = conf.ifc_len / static_cast<int>(sizeof(struct ifreq));
        struct ifreq* reqs = reinterpret_cast<struct ifreq*>(buf);
        for (int i = 0; i < count; ++i) {
            if (reqs[i].ifr_addr.sa_family != AF_INET) continue;
            Interface itf;
            itf.name = reqs[i].ifr_name;
            itf.address = ntohl(reinterpret_cast<sockaddr_in*>(&reqs[i].ifr_addr)->sin_addr.s_addr);
            struct ifreq req;
            std::memset(&req, 0, sizeof(req));
            std::strncpy(req.ifr_name, reqs[i].ifr_name, IFNAMSIZ - 1);
            if (ioctl(s, SIOCGIFFLAGS, &req) == 0) {
                if (!(req.ifr_flags & IFF_UP)) continue;
                itf.loopback = (req.ifr_flags & IFF_LOOPBACK) != 0;
            }
            if ((itf.address >> 24) == 127) itf.loopback = true;
            std::memset(&req, 0, sizeof(req));
            std::strncpy(req.ifr_name, reqs[i].ifr_name, IFNAMSIZ - 1);
            if (ioctl(s, SIOCGIFNETMASK, &req) == 0) {
                itf.netmask = ntohl(reinterpret_cast<sockaddr_in*>(&req.ifr_netmask)->sin_addr.s_addr);
            }
            if (itf.address != 0) result.push_back(itf);
        }
    }
    close(s);
#endif
    return result;
}

uint32_t primaryAddress() {
    const std::vector<Interface> all = ipv4Interfaces();
    auto isPrivate = [](uint32_t a) {
        return (a >> 24) == 10 || (a >> 20) == ((172u << 4) | 1u) || (a >> 16) == ((192u << 8) | 168u);
    };
    auto isLinkLocal = [](uint32_t a) { return (a >> 16) == ((169u << 8) | 254u); };
    for (const Interface& i : all) {
        if (!i.loopback && isPrivate(i.address)) return i.address;
    }
    for (const Interface& i : all) {
        if (!i.loopback && !isLinkLocal(i.address)) return i.address;
    }
    return 0x7f000001u;
}

std::string deviceName() {
#ifdef _WIN32
    wchar_t wide[256];
    DWORD size = 256;
    if (GetComputerNameExW(ComputerNamePhysicalDnsHostname, wide, &size) && size > 0) {
        char utf8[512];
        const int n = WideCharToMultiByte(CP_UTF8, 0, wide, static_cast<int>(size), utf8, sizeof(utf8), nullptr, nullptr);
        if (n > 0) {
            std::string name(utf8, static_cast<size_t>(n));
            for (char& c : name) {
                if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');   // like the NetBIOS name
            }
            return name;
        }
    }
    return "PC";
#elif defined(__ANDROID__)
    char model[PROP_VALUE_MAX] = {0};
    char maker[PROP_VALUE_MAX] = {0};
    __system_property_get("ro.product.model", model);
    __system_property_get("ro.product.manufacturer", maker);
    std::string m = model;
    std::string k = maker;
    if (m.empty()) return "Android";
    if (!k.empty() && m.compare(0, k.size(), k) != 0 && k.size() + m.size() < 40) {
        if (k[0] >= 'a' && k[0] <= 'z') k[0] = static_cast<char>(k[0] - 'a' + 'A');
        return k + " " + m;
    }
    return m;
#else
    char host[256] = {0};
    if (gethostname(host, sizeof(host) - 1) == 0 && host[0]) return host;
    return "Device";
#endif
}

std::string ipToString(uint32_t a) {
    char buf[20];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (a >> 24) & 255u, (a >> 16) & 255u, (a >> 8) & 255u, a & 255u);
    return buf;
}

bool parseIPv4(const std::string& text, uint32_t* out) {
    unsigned int p[4];
    char tail = 0;
    if (std::sscanf(text.c_str(), "%u.%u.%u.%u%c", &p[0], &p[1], &p[2], &p[3], &tail) != 4) return false;
    for (unsigned int v : p) {
        if (v > 255) return false;
    }
    if (out) *out = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    return true;
}

void setMulticastLock(bool held) {
#if defined(__ANDROID__)
    cocos2d::JniHelper::callStaticVoidMethod("org/openwheels/game/AppActivity", "setMulticastLock", held);
#else
    (void)held;
#endif
}

}  // namespace net
