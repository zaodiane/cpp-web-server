#include "Acceptor.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>

void setNonBlocking(int fd){
    int flags = fcntl(fd,F_GETFL,0);
    fcntl(fd,F_SETFL,flags |O_NONBLOCK);
}

Acceptor::Acceptor(EventLoop* loop,int port):
    loop_(loop),lfd_(-1),channel_(nullptr){
        lfd_ = socket(AF_INET,SOCK_STREAM,0);
        if(lfd_ < 0){
            perror("socket error");
            return;
        }

        int opt = 1;
        setsockopt(lfd_,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

        addr_.sin_family = AF_INET;
        addr_.sin_port = htons(port);
        addr_.sin_addr.s_addr = INADDR_ANY;

        if(bind(lfd_,(struct sockaddr*)&addr_,sizeof(addr_))<0){
            perror("bind error");
            close(lfd_);
            return;
        }

        setNonBlocking(lfd_);

        channel_ = new Channel(loop_,lfd_);
        channel_->enableRead();
        channel_->enableET();
        channel_->setReadCallback([this](){ handleAccept(); });
    }

Acceptor::~Acceptor(){
    delete channel_;
    close(lfd_);
}

void Acceptor::listen(){
    if(::listen(lfd_,128)<0){
        perror("listen error");
        return;
    }
    loop_->updateChannel(channel_);
    std::cout << "Acceptor is listening on port " << 
                ntohs(addr_.sin_port) << std::endl;
}

void Acceptor::handleAccept(){
    while(true){
        int cfd = accept(lfd_,NULL,NULL);
        if(cfd < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                std::cout << "read over" << std::endl;
                break;
            }else{
                perror("accept error");
                break;
            }
        }
        setNonBlocking(cfd);
        if(newConnectionCallback_){
            newConnectionCallback_(cfd);
        }
    }
}
