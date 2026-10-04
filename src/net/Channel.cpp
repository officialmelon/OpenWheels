// NET (PC addition): framed TCP channel and listener, see Channel.h.
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
#include <cerrno>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "net/Channel.h"

#include <cstring>

namespace net {

namespace {

#ifdef _WIN32
using SockLen = int;
const int kSendFlags = 0;
#else
using SockLen = socklen_t;
const int kSendFlags = MSG_NOSIGNAL;
#endif

sockaddr_in makeAddress(uint32_t ip, uint16_t port) {
    sockaddr_in a;
    std::memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(ip);
    a.sin_port = htons(port);
    return a;
}

void setNoDelay(SocketHandle s) {
    int on = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
}

// 0 = still connecting, 1 = connected, -1 = failed (error in *error).
int pollConnect(SocketHandle s, int* error) {
    fd_set ws, es;
    FD_ZERO(&ws);
    FD_ZERO(&es);
    FD_SET(s, &ws);
    FD_SET(s, &es);
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    const int r = select(static_cast<int>(s + 1), nullptr, &ws, &es, &tv);
    if (r < 0) {
        *error = lastError();
        return -1;
    }
    if (r == 0) return 0;
    int soError = 0;
    SockLen len = sizeof(soError);
    getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&soError), &len);
    if (FD_ISSET(s, &es) || soError != 0) {
        *error = soError;
        return -1;
    }
    return FD_ISSET(s, &ws) ? 1 : 0;
}

}  // namespace

// ---- Channel ---------------------------------------------------------------------------------

std::shared_ptr<Channel> Channel::connect(uint32_t ip, uint16_t port, double connectTimeout) {
    std::shared_ptr<Channel> channel(new Channel());
    channel->_remoteAddress = ip;
    channel->_remotePort = port;
    channel->_connectTimeout = connectTimeout;
    {
        std::lock_guard<std::mutex> lock(channel->_mutex);
        if (!startup()) {
            channel->fail("networking is not available");
        } else {
            SocketHandle s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (s == kInvalidSocket) {
                channel->fail(lastErrorText());
            } else {
                setNonBlocking(s);
                setNoDelay(s);
                sockaddr_in a = makeAddress(ip, port);
                channel->_socket = s;
                if (::connect(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0 && !lastErrorWouldBlock()) {
                    channel->fail("could not connect (" + lastErrorText() + ")");
                }
            }
        }
    }
    NetLoop::instance()->add(channel);
    return channel;
}

Channel::~Channel() {
    closeSocket(_socket);
}

void Channel::setHandler(const Handler& handler) {
    _handler = handler;
    _hasHandler = true;
    dispatch();
}

void Channel::clearHandler() {
    _handler = Handler();
    _hasHandler = false;
}

void Channel::setMaxPayload(size_t bytes) {
    std::lock_guard<std::mutex> lock(_mutex);
    _maxPayload = bytes;
}

void Channel::setIdleTimeout(double seconds) {
    std::lock_guard<std::mutex> lock(_mutex);
    _idleTimeout = seconds;
}

void Channel::send(const Message& message) {
    std::string bytes = frame::encode(message);
    std::lock_guard<std::mutex> lock(_mutex);
    if (_state == State::Closed || _state == State::Closing) return;
    _out += bytes;
}

void Channel::close() {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_state == State::Connecting) _abort = true;
    else if (_state == State::Open) _state = State::Closing;
}

void Channel::abort() {
    std::lock_guard<std::mutex> lock(_mutex);
    _abort = true;
}

bool Channel::isOpen() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _state == State::Open;
}

std::string Channel::remoteText() const {
    return ipToString(_remoteAddress) + ":" + std::to_string(_remotePort);
}

void Channel::collect(std::vector<SocketHandle>& read, std::vector<SocketHandle>& write) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_socket == kInvalidSocket || _state == State::Closed) return;
    read.push_back(_socket);
    if (_state == State::Connecting || !_out.empty()) write.push_back(_socket);
}

bool Channel::finished() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _state == State::Closed;
}

void Channel::queueEvent(Event event) {
    _events.push_back(std::move(event));
    if (!_dispatchPosted) {
        _dispatchPosted = true;
        std::weak_ptr<Channel> weak = shared_from_this();
        NetLoop::instance()->post([weak]() {
            if (auto self = weak.lock()) self->dispatch();
        });
    }
}

void Channel::dispatch() {
    // Cocos thread. Keep ourselves alive while handlers run (they may drop their reference).
    std::shared_ptr<Channel> self = shared_from_this();
    for (;;) {
        Event event;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _dispatchPosted = false;
            if (!_hasHandler || _events.empty()) return;
            event = std::move(_events.front());
            _events.pop_front();
        }
        Handler handler = _handler;
        switch (event.kind) {
        case Event::Open:
            if (handler.onOpen) handler.onOpen();
            break;
        case Event::Message:
            if (handler.onMessage) handler.onMessage(event.message);
            break;
        case Event::Closed:
            if (handler.onClosed) handler.onClosed(event.error);
            break;
        }
    }
}

void Channel::fail(const std::string& error) {
    finishClose(error.empty() ? std::string("connection failed") : error);
}

void Channel::finishClose(const std::string& error) {
    if (_state == State::Closed) return;
    _state = State::Closed;
    closeSocket(_socket);
    _socket = kInvalidSocket;
    _out.clear();
    Event e{Event::Closed, Message(), error};
    queueEvent(std::move(e));
}

void Channel::process(double now) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_state == State::Closed) return;
    if (_abort) {
        finishClose("");
        return;
    }
    if (_state == State::Connecting) {
        if (_connectDeadline == 0.0) _connectDeadline = now + _connectTimeout;
        int error = 0;
        const int r = pollConnect(_socket, &error);
        if (r < 0) {
            fail("could not connect (socket error " + std::to_string(error) + ")");
            return;
        }
        if (r == 0) {
            if (now > _connectDeadline) fail("connection timed out");
            return;
        }
        _state = State::Open;
        _lastReceive = now;
        queueEvent(Event{Event::Open, Message(), std::string()});
    }

    // receive
    char buf[65536];
    for (;;) {
        const int n = static_cast<int>(recv(_socket, buf, sizeof(buf), 0));
        if (n > 0) {
            _in.append(buf, static_cast<size_t>(n));
            _lastReceive = now;
            if (_in.size() > _maxPayload + frame::kHeaderSize + 65536) break;   // decode before reading more
            continue;
        }
        if (n == 0) {
            // Peer closed; deliver whatever complete frames arrived first.
            for (;;) {
                Message m;
                size_t used = 0;
                std::string error;
                if (frame::decode(_in, _maxPayload, &m, &used, &error) != frame::DecodeResult::Ok) break;
                _in.erase(0, used);
                queueEvent(Event{Event::Message, std::move(m), std::string()});
            }
            finishClose("");
            return;
        }
        if (lastErrorWouldBlock()) break;
        finishClose("connection lost (" + lastErrorText() + ")");
        return;
    }
    for (;;) {
        Message m;
        size_t used = 0;
        std::string error;
        const frame::DecodeResult r = frame::decode(_in, _maxPayload, &m, &used, &error);
        if (r == frame::DecodeResult::NeedMore) break;
        if (r == frame::DecodeResult::Error) {
            finishClose(error);
            return;
        }
        _in.erase(0, used);
        queueEvent(Event{Event::Message, std::move(m), std::string()});
    }

    // send
    while (!_out.empty()) {
        const int chunk = static_cast<int>(_out.size() > 262144 ? 262144 : _out.size());
        const int n = static_cast<int>(::send(_socket, _out.data(), chunk, kSendFlags));
        if (n > 0) {
            _out.erase(0, static_cast<size_t>(n));
            continue;
        }
        if (n < 0 && lastErrorWouldBlock()) break;
        finishClose("connection lost (" + lastErrorText() + ")");
        return;
    }
    if (_state == State::Closing && _out.empty()) {
        finishClose("");
        return;
    }
    if (_idleTimeout > 0.0 && _lastReceive >= 0.0 && now - _lastReceive > _idleTimeout) {
        finishClose("timed out");
    }
}

// ---- Listener --------------------------------------------------------------------------------

std::shared_ptr<Listener> Listener::open(uint16_t firstPort, int tries, AcceptCallback onAccept) {
    if (!startup()) return nullptr;
    for (int i = 0; i < tries; ++i) {
        const uint16_t port = static_cast<uint16_t>(firstPort + i);
        SocketHandle s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == kInvalidSocket) return nullptr;
#ifdef _WIN32
        // Without this a second instance could bind the same port (SO_REUSEADDR semantics).
        BOOL exclusive = TRUE;
        setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
#else
        int on = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif
        sockaddr_in a = makeAddress(0, port);   // INADDR_ANY
        if (bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0 || listen(s, 8) != 0) {
            closeSocket(s);
            continue;
        }
        setNonBlocking(s);
        std::shared_ptr<Listener> listener(new Listener());
        listener->_socket = s;
        listener->_port = port;
        listener->_onAccept = std::move(onAccept);
        NetLoop::instance()->add(listener);
        return listener;
    }
    return nullptr;
}

Listener::~Listener() {
    closeSocket(_socket);
}

void Listener::close() {
    std::lock_guard<std::mutex> lock(_mutex);
    _closed = true;
}

bool Listener::finished() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _closed;
}

void Listener::collect(std::vector<SocketHandle>& read, std::vector<SocketHandle>&) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_closed) read.push_back(_socket);
}

void Listener::process(double now) {
    std::lock_guard<std::mutex> lock(_mutex);
    if (_closed) return;
    for (int i = 0; i < 8; ++i) {
        sockaddr_in a;
        SockLen len = sizeof(a);
        SocketHandle s = accept(_socket, reinterpret_cast<sockaddr*>(&a), &len);
        if (s == kInvalidSocket) break;
        setNonBlocking(s);
        setNoDelay(s);
        std::shared_ptr<Channel> channel(new Channel());
        channel->_socket = s;
        channel->_state = Channel::State::Open;
        channel->_remoteAddress = ntohl(a.sin_addr.s_addr);
        channel->_remotePort = ntohs(a.sin_port);
        channel->_lastReceive = now;
        NetLoop::instance()->add(channel);
        AcceptCallback cb = _onAccept;
        NetLoop::instance()->post([cb, channel]() {
            if (cb) cb(channel);
            else channel->abort();
        });
    }
}

}  // namespace net
