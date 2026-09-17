#include "Channel.h"
#include "EventLoop.h"

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0) {}

void Channel::handleEvent() {
    if (revents_ & EPOLLIN) {
        if (readCallback_) readCallback_();
    }
}

void Channel::enableRead() {
    events_ |= EPOLLIN;
    loop_->updateChannel(this);
}

void Channel::enableET() {
    events_ |= EPOLLET;
    loop_->updateChannel(this);
}