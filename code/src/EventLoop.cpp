#include "EventLoop.h"
#include "Channel.h"
#include <unistd.h>
#include <iostream>
#include <sys/eventfd.h>

EventLoop::EventLoop() {
    epfd_ = epoll_create(1);
    if (epfd_ < 0) {
        perror("epoll_create error");
        exit(-1);
    }

    wakeupFd_ = eventfd(0,EFD_NONBLOCK);
    if(wakeupFd_ < 0){
        perror("eventfd error");
        exit(-1);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = wakeupFd_;
    epoll_ctl(epfd_,EPOLL_CTL_ADD,wakeupFd_,&ev);
}

EventLoop::~EventLoop() {
    close(epfd_);
}

void EventLoop::updateChannel(Channel* ch) {
    struct epoll_event ev;
    ev.events = ch->events();
    ev.data.ptr = ch;
    int fd = ch->fd();

    if (channels_.find(fd) == channels_.end()) {
        epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    } else {
        epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
    }
    channels_[fd] = ch;
}

void EventLoop::removeChannel(Channel* ch) {
    int fd = ch->fd();
    epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, nullptr);
    channels_.erase(fd);
}

void EventLoop::loop() {
    while (!quit_) {
        int nfds = epoll_wait(epfd_, events_, 1024, -1);
        for (int i = 0; i < nfds; ++i) {
            if(events_[i].data.fd == wakeupFd_){
                uint64_t val;
                read(wakeupFd_,&val,sizeof(val));
                doPendingFunctors();
                continue;
            }

            Channel* ch = static_cast<Channel*>(events_[i].data.ptr);
            if (ch == nullptr) continue;
            ch->setRevents(events_[i].events);
            ch->handleEvent();
        }
    }
}

void EventLoop::quit(){
    quit_ = true;
    wakeup();
}

void EventLoop::runInLoop(std::function<void()> cb){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push(cb);
    }
    wakeup();
}

void EventLoop::wakeup(){
    uint64_t val = 1;
    write(wakeupFd_,&val,sizeof(val));
}

void EventLoop::doPendingFunctors(){
    std::queue<std::function<void()>> functors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::swap(functors,pendingFunctors_);
    }
    while(!functors.empty()){
        functors.front()();
        functors.pop();
    }
}