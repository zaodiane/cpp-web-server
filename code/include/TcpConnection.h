#pragma once
#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <ctime>
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

        time_t lastActiveTime() const{ return last_active_time_; }
        void forceClose() { handleClose(); }

    private:
        int id_;
        EventLoop* loop_;
        int fd_;
        Channel* channel_;
        CloseCallback closeCallback_;

        std::atomic<bool> closed_;
        std::string read_buffer_;
        std::atomic<time_t> last_active_time_;
        
        void handleRead();
        void handleClose();

        std::string handleLogin(const std::string& req_body, std::string& extra_headers);
        std::string handleRegister(const std::string& req_body);
        std::string handleGetFile(const std::string& path);
        std::string buildResponse(const std::string& body, const std::string& status_line = "HTTP/1.1 200 OK\r\n",
                                    const std::string& extra_headers = "");

        std::string getCookieValue(const std::string& cookie_str, const std::string& key);
        std::string handleProfile(const std::string& cookie_str);
};