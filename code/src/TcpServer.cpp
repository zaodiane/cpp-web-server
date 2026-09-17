#include "TcpServer.h"
#include <iostream>

TcpServer::TcpServer(EventLoop* loop,int port,int subcount)
    :loop_(loop),acceptor_(loop,port),nextSub_(0){
        for(int i=0;i<subcount;i++){
            EventLoop* subLoop = new EventLoop();
            subLoops_.push_back(subLoop);
            subThreads_.emplace_back([subLoop](){
                subLoop->loop();
            });
        }

        acceptor_.setNewConnectionCallback([this](int fd){
            onNewConnection(fd);
        });
    }

TcpServer::~TcpServer(){
    connections_.clear();

    for(auto* subLoop : subLoops_){
        subLoop->quit();
        delete subLoop;
    }

    for(auto& t : subThreads_){
        if(t.joinable()) t.join();
    }
}

void TcpServer::start(){
    acceptor_.listen();
}

void TcpServer::onNewConnection(int fd){
    std::cout << "TcpServer: New connection fd = " << fd << std::endl;

    EventLoop* subLoop = subLoops_[nextSub_];
    nextSub_ = (nextSub_ + 1) % subLoops_.size();

    auto conn = std::make_shared<TcpConnection>(subLoop,fd);
    int id = nextConnId_++;
    conn->setId(id);
    conn->setCloseCallback([this](TcpConnection* c){
        onCloseConnection(c);
    });
    subLoop->runInLoop([conn](){
        conn->start();
    });

    connections_[id] = conn;    
}

void TcpServer::onCloseConnection(TcpConnection* conn){
    int id = conn->id();

    loop_->runInLoop([this, id]() {
        auto it = connections_.find(id);
        if (it != connections_.end()) {
            connections_.erase(it);
        }
    });
}
