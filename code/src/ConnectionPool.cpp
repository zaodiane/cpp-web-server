#include "ConnectionPool.h"
#include <iostream>

ConnectionPool::~ConnectionPool() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    
    std::lock_guard<std::mutex> lock(mtx_);
    while (!connQueue_.empty()) {
        mysqlConn* conn = connQueue_.front();
        connQueue_.pop();
        delete conn;
    }
    std::cout << "[ConnectionPool] 销毁，所有连接已释放。" << std::endl;
}

ConnectionPool&  ConnectionPool:: getInstance(){
    static ConnectionPool instance;
    return instance;
}

mysqlConn* ConnectionPool:: getConnection(){
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock,[this](){
        return !connQueue_.empty() || stop_;
    });

    if(stop_ && connQueue_.empty()){
        return nullptr;
    }
    mysqlConn* conn = connQueue_.front();
    connQueue_.pop();

    return conn;
}

void ConnectionPool:: returnConnection(mysqlConn* conn){
    if(!conn) return;

    std::lock_guard<std::mutex> lock(mtx_);
    connQueue_.push(conn);
    cv_.notify_one();
}

void ConnectionPool:: init(const std::string& host,const std::string& user,
            const std::string& password,const std::string& db,
            int initSize)
    {
        for(int i=0;i<initSize;i++){
            mysqlConn* conn = new mysqlConn(host,user,password,db);
            connQueue_.push(conn);
        }
        std::cout << "[ConnectionPool] 初始化完成，创建了 " << initSize << " 个连接。" << std::endl;
    }