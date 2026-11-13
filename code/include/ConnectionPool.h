#pragma once
#include<queue>
#include<mutex>
#include<atomic>
#include<string>
#include<condition_variable>
#include"mysqlConn.h"

class ConnectionPool{
    public:
        static ConnectionPool& getInstance();

        mysqlConn* getConnection();

        void returnConnection(mysqlConn* conn);

        void init(const std::string& host,const std::string& user,
                    const std::string& password,const std::string& db,
                    int initSize = 5);
    
    private:
        ConnectionPool() = default;
        ~ConnectionPool();

        ConnectionPool(const ConnectionPool&) = delete;
        ConnectionPool& operator=(const ConnectionPool&) = delete;

        std::queue<mysqlConn*> connQueue_;
        std::mutex mtx_;
        std::condition_variable cv_;
        std::atomic<bool> stop_{false};
};