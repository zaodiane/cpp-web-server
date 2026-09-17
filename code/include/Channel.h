#pragma once                     //预处理指令，作用是防止头文件被重复包含
#include <functional>
#include <sys/epoll.h>

class EventLoop;

class Channel {
public:
    using Callback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel() = default;

    void handleEvent();
    void enableRead();
    void enableET();
    void setReadCallback(Callback cb) { readCallback_ = cb; }

    int fd() const { return fd_; }
    uint32_t events() const { return events_; }
    void setRevents(uint32_t rev) { revents_ = rev; }

private:
    EventLoop* loop_;
    int fd_;
    uint32_t events_;
    uint32_t revents_;
    Callback readCallback_;
};