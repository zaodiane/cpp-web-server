#pragma once
#include <functional>
#include <netinet/in.h>
#include "EventLoop.h"
#include "Channel.h"

class Acceptor {
    public:
        using NewConnectionCallback = std::function<void(int)>;
        
        Acceptor(EventLoop* loop,int port);
        ~Acceptor();
        
        void setNewConnectionCallback(NewConnectionCallback cb){
            newConnectionCallback_ = cb;
        }

        void listen();

    private:
        EventLoop* loop_;
        int lfd_;
        Channel* channel_;
        struct sockaddr_in addr_;
        NewConnectionCallback newConnectionCallback_;

        void handleAccept();
};