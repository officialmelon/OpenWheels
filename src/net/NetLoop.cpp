// NET (PC addition): the network worker thread, see NetLoop.h.
#ifdef _WIN32
// fd_set holds 64 sockets by default on Windows; the loop may wait on more than that.
#define FD_SETSIZE 256
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif

#include "net/NetLoop.h"

#include <algorithm>
#include <chrono>

#include "cocos2d.h"

namespace net {

NetLoop* NetLoop::instance() {
    // Never destroyed: a joinable std::thread in a static destructor would call std::terminate.
    static NetLoop* loop = new NetLoop();
    return loop;
}

double NetLoop::now() {
    using namespace std::chrono;
    static const steady_clock::time_point start = steady_clock::now();
    return duration_cast<duration<double>>(steady_clock::now() - start).count();
}

NetLoop::NetLoop() {
    startup();
    _running = true;
    _thread = std::thread([this]() { run(); });
    cocos2d::Director::getInstance()->getScheduler()->schedule([this](float) { pump(); }, this, 0.0f, false,
                                                                "net_loop_pump");
}

void NetLoop::add(const std::shared_ptr<Pollable>& pollable) {
    std::lock_guard<std::mutex> lock(_mutex);
    _added.push_back(pollable);
}

void NetLoop::post(std::function<void()> fn) {
    std::lock_guard<std::mutex> lock(_postMutex);
    _posted.push_back(std::move(fn));
}

void NetLoop::pump() {
    std::vector<std::function<void()>> todo;
    {
        std::lock_guard<std::mutex> lock(_postMutex);
        todo.swap(_posted);
    }
    for (auto& fn : todo) fn();
}

void NetLoop::shutdown() {
    if (!_running.exchange(false)) return;
    if (_thread.joinable()) _thread.join();
    std::lock_guard<std::mutex> lock(_postMutex);
    _posted.clear();
}

void NetLoop::run() {
    std::vector<SocketHandle> readSockets, writeSockets;
    while (_running) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            for (auto& p : _added) _pollables.push_back(p);
            _added.clear();
        }
        readSockets.clear();
        writeSockets.clear();
        for (auto& p : _pollables) p->collect(readSockets, writeSockets);

        fd_set rs, ws;
        FD_ZERO(&rs);
        FD_ZERO(&ws);
        SocketHandle maxSocket = 0;
        int count = 0;
        for (SocketHandle s : readSockets) {
            if (s == kInvalidSocket || count >= FD_SETSIZE - 1) continue;
            FD_SET(s, &rs);
            maxSocket = std::max(maxSocket, s);
            ++count;
        }
        for (SocketHandle s : writeSockets) {
            if (s == kInvalidSocket || count >= FD_SETSIZE - 1) continue;
            FD_SET(s, &ws);
            maxSocket = std::max(maxSocket, s);
            ++count;
        }
        if (count == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        } else {
            timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 15000;
            select(static_cast<int>(maxSocket + 1), &rs, &ws, nullptr, &tv);
        }

        const double t = now();
        for (auto& p : _pollables) p->process(t);
        _pollables.erase(std::remove_if(_pollables.begin(), _pollables.end(),
                                        [](const std::shared_ptr<Pollable>& p) { return p->finished(); }),
                         _pollables.end());
    }
}

}  // namespace net
