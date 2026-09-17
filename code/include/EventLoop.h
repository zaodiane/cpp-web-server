#pragma once
#include <vector>
#include <queue>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <sys/epoll.h>

class Channel;

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    void loop();
    void updateChannel(Channel* ch);
    void removeChannel(Channel* ch);

    void quit();
    void runInLoop(std::function<void()> cb);
    void wakeup();

private:
    int epfd_;
    struct epoll_event events_[1024];
    std::unordered_map<int, Channel*> channels_;

    bool quit_;
    int wakeupFd_;
    std::queue<std::function<void()>> pendingFunctors_;
    std::mutex mutex_;

    void doPendingFunctors();
};