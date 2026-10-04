#pragma once
// NET (PC addition): a framed message channel over TCP (Channel) and a TCP listener that hands
// out incoming channels (Listener). Generic: the level transfer (LevelTransfer) is one protocol on
// top of it, and a later ghost-race mode can be another.
//
// Threading: socket work happens on the NetLoop worker; every callback (Handler, accept) runs on
// the cocos thread. send() and close() may be called from any thread.
//
// Handshake: none at this level. Protocols start with their own "hello" message (see
// LevelTransfer.h) so they can check the app id and protocol version.

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "net/Message.h"
#include "net/NetLoop.h"

namespace net {

class Channel : public Pollable, public std::enable_shared_from_this<Channel> {
public:
    struct Handler {
        std::function<void()> onOpen;                              // connected (outgoing only)
        std::function<void(const Message&)> onMessage;
        std::function<void(const std::string& error)> onClosed;    // error "" = closed normally
    };

    // Starts a non-blocking connect to ip:port (host order); fails with onClosed after
    // `connectTimeout` seconds.
    static std::shared_ptr<Channel> connect(uint32_t ip, uint16_t port, double connectTimeout = 8.0);

    // Cocos thread. Events that arrived before a handler was set are delivered now.
    void setHandler(const Handler& handler);
    void clearHandler();
    // Largest accepted incoming payload (default 64 KiB). Larger frames close the channel.
    void setMaxPayload(size_t bytes);
    // Closes the channel when nothing was received for `seconds` (0 = never; default 0).
    void setIdleTimeout(double seconds);

    void send(const Message& message);
    // Flushes what was queued, then closes. onClosed("") follows.
    void close();
    // Drops the connection immediately. onClosed("") follows.
    void abort();

    bool isOpen() const;
    uint32_t remoteAddress() const { return _remoteAddress; }
    uint16_t remotePort() const { return _remotePort; }
    std::string remoteText() const;

    // Pollable (worker thread)
    void collect(std::vector<SocketHandle>& read, std::vector<SocketHandle>& write) override;
    void process(double now) override;
    bool finished() const override;

    ~Channel() override;

private:
    friend class Listener;
    enum class State { Connecting, Open, Closing, Closed };
    struct Event {
        enum Kind { Open, Message, Closed } kind;
        net::Message message;
        std::string error;
    };

    Channel() = default;
    void fail(const std::string& error);         // worker, _mutex held
    void finishClose(const std::string& error);  // worker, _mutex held
    void queueEvent(Event event);                // worker, _mutex held
    void dispatch();                             // cocos thread

    mutable std::mutex _mutex;
    SocketHandle _socket = kInvalidSocket;
    State _state = State::Connecting;
    uint32_t _remoteAddress = 0;
    uint16_t _remotePort = 0;
    double _connectDeadline = 0.0;
    double _connectTimeout = 8.0;
    double _idleTimeout = 0.0;
    double _lastReceive = -1.0;
    size_t _maxPayload = 64 * 1024;
    std::string _in;
    std::string _out;
    bool _abort = false;
    std::deque<Event> _events;                   // worker -> cocos
    bool _dispatchPosted = false;
    Handler _handler;                            // cocos thread only
    bool _hasHandler = false;                    // cocos thread only
};

class Listener : public Pollable, public std::enable_shared_from_this<Listener> {
public:
    using AcceptCallback = std::function<void(const std::shared_ptr<Channel>& channel)>;

    // Binds the first free TCP port of firstPort .. firstPort + tries - 1 on all interfaces and
    // listens. nullptr when none is free. onAccept runs on the cocos thread.
    static std::shared_ptr<Listener> open(uint16_t firstPort, int tries, AcceptCallback onAccept);
    uint16_t port() const { return _port; }
    void close();

    void collect(std::vector<SocketHandle>& read, std::vector<SocketHandle>& write) override;
    void process(double now) override;
    bool finished() const override;
    ~Listener() override;

private:
    Listener() = default;
    mutable std::mutex _mutex;
    SocketHandle _socket = kInvalidSocket;
    uint16_t _port = 0;
    bool _closed = false;
    AcceptCallback _onAccept;
};

}  // namespace net
