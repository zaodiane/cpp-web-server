#include "TcpConnection.h"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <cerrno>
#include <thread>

std::string readFile(const std::string& path){
    std::string filename = "static" + path;
    std::ifstream file(filename);
    if(!file.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

TcpConnection::TcpConnection(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), channel_(nullptr), closed_(false), id_(0) {
    channel_ = new Channel(loop_, fd_);
    channel_->enableRead();
    channel_->enableET();
}

TcpConnection::~TcpConnection() {
    if (channel_) {
        delete channel_;
        channel_ = nullptr;
    }
}

void TcpConnection::start(){
    auto self = shared_from_this();
    channel_->setReadCallback([self]() { self->handleRead(); });
    loop_->updateChannel(channel_);
}

void TcpConnection::handleRead(){
    if(closed_)  return;
    
    char buffer[1024] = {0};
    while(true){
        memset(buffer,0,sizeof(buffer));
        int valread = recv(fd_,buffer,sizeof(buffer),0);
        if(valread < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }else{
                perror("recv error");
                handleClose();
                return;
            }
        }else if(valread == 0){
            std::cout << "client disconnected: " << fd_ << std::endl;
            handleClose();
            return;
        }else{
            char method[16], path[256], version[16];
            sscanf(buffer, "%s %s %s", method, path, version);
            std::cout<< "path: " << path << std::endl;

            std::string req_path = path;
            if(req_path == "/") req_path = "/index.html";
            
            std::string body = readFile(req_path);
            std::string response;

            if(!body.empty()){
                response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: " + std::to_string(body.size()) + "\r\n"
                           "\r\n" + body;
            }else{
                std::string not_found = "<h1>404 Not Found</h1>";
                response = "HTTP/1.1 404 Not Found\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: " + std::to_string(not_found.size()) + "\r\n"
                           "\r\n" + not_found;
            }
            ::send(fd_,response.c_str(),response.size(),0);
            handleClose();
            std::cout << "after handleClose" << std::endl;
            return;
        }
    }
}

void TcpConnection::handleClose(){
    if(closed_.exchange(true))  return;
    closed_ = true;
    
    if (channel_) {
        loop_->removeChannel(channel_);
        delete channel_;
        channel_ = nullptr;
    }

    close(fd_);
    if(closeCallback_){
        closeCallback_(this);
    }
}