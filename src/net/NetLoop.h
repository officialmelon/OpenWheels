#pragma once
// NET (PC addition): the network worker thread behind the LAN features.
//
// One background thread waits on every registered socket (select, 15 ms timeout) and lets each
// Pollable do its non-blocking socket work; nothing in src/net/ ever blocks the cocos thread.
// Results reach the game through post(): the functions are queued and run on the cocos thread
// by a Scheduler callback once per frame. (The queue is drained by the scheduler rather than with
// Scheduler::performFunctionInCocosThread from the worker so the worker never touches the
// Director, which is destroyed before static destructors run at exit.)

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "net/NetPlatform.h"

namespace net {

// Something the worker thread services. All three methods run on the worker thread.
class Pollable {
public:
    virtual ~Pollable() = default;
    // Sockets to wait on (readability) and those with data to write (writability).
    virtual void collect(std::vector<SocketHandle>& read, std::vector<SocketHandle>& write) = 0;
    // Non-blocking socket work; `now` = seconds on a monotonic clock (NetLoop::now()).
    virtual void process(double now) = 0;
    // True once the object is finished; the loop then drops it.
    virtual bool finished() const = 0;
};

class NetLoop {
public:
    // Created on first use, which must be on the cocos thread (it installs the scheduler pump).
    static NetLoop* instance();
    static double now();

    void add(const std::shared_ptr<Pollable>& pollable);   // any thread
    // Runs `fn` on the cocos thread during the next frame. Any thread.
    void post(std::function<void()> fn);
    // Stops the worker and joins it (AppDelegate destructor). Pending posts are dropped.
    void shutdown();
    bool running() const { return _running; }

private:
    NetLoop();
    void run();
    void pump();   // cocos thread

    std::thread _thread;
    std::atomic<bool> _running{false};
    std::mutex _mutex;                                        // _pollables, _added
    std::vector<std::shared_ptr<Pollable>> _pollables;        // worker-owned
    std::vector<std::shared_ptr<Pollable>> _added;            // handed over by add()
    std::mutex _postMutex;
    std::vector<std::function<void()>> _posted;
};

}  // namespace net
