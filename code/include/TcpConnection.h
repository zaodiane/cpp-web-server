#pragma once
#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include "EventLoop.h"
#include "Channel.h"

class TcpConnection : public std::enable_shared_from_this<TcpConnection>{
     public:
        using CloseCallback = std::function<void(TcpConnection*)>;

        TcpConnection(EventLoop* loop, int fd);
        ~TcpConnection();

        int fd() const { return fd_; };
        void setCloseCallback(CloseCallback cb) { closeCallback_ = cb; }
        void start();
        int isClose() { return closed_;}

        void setId(int id) { id_ = id; }
        int id() const { return id_; }

    private:
        int id_;
        EventLoop* loop_;
        int fd_;
        Channel* channel_;
        CloseCallback closeCallback_;

        std::atomic<bool> closed_;
        
        void handleRead();
        void handleClose();
};