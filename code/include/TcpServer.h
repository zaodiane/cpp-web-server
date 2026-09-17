#pragma once
#include "Acceptor.h"
#include "TcpConnection.h"
#include <vector>
#include <thread>
#include <memory>
#include <unordered_map>

class TcpServer{
    public:
        TcpServer(EventLoop* loop,int port,int subcount = 4);
        ~TcpServer();
        
        void start();

    private:
        EventLoop* loop_;
        Acceptor acceptor_;
        std::unordered_map<int,std::shared_ptr<TcpConnection>> connections_;
        int nextConnId_ = 0;

        std::vector<EventLoop*> subLoops_;
        std::vector<std::thread> subThreads_;
        int nextSub_;

        void onNewConnection(int fd);
        void onCloseConnection(TcpConnection* conn);
};
