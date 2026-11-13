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

        timer_thread_ = std::thread([this](){
            while(!quit_){
                std::this_thread::sleep_for(std::chrono::seconds(5));
                if(quit_) break;
                loop_->runInLoop([this](){
                    time_t now = time(nullptr);
                    std::vector<int> timeout_ids;

                    for(const auto& pair: connections_){
                        int id = pair.first;
                        auto conn = pair.second;
                        if(now - conn->lastActiveTime() > 60){
                            timeout_ids.push_back(id);
                        }
                    }
                    // for (int id : timeout_ids) {
                    //     auto it = connections_.find(id);
                    //     if (it != connections_.end()) {
                    //         std::cout << "【定时器】清理超时连接 id=" << id << std::endl;
                    //         it->second->forceClose(); 
                    //     }
                    // }
                    for (int id : timeout_ids) {
                        auto it = connections_.find(id);
                        if (it != connections_.end()) {
                            // 【重点：这里加了超时时间打印】
                            time_t last_time = it->second->lastActiveTime();
                            long long diff = now - last_time;
                            
                            std::cout << "【定时器】清理超时连接 id=" << id 
                                      << " | 当前时间: " << now 
                                      << " | 最后活跃: " << last_time 
                                      << " | 已超时: " << diff << " 秒" << std::endl;
                                      
                            it->second->forceClose(); 
                        }
                    }
                });
            }
        });
    }

TcpServer::~TcpServer(){
    quit_ = true;
    if(timer_thread_.joinable()){
        timer_thread_.join();
    }

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
