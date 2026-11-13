#include "TcpConnection.h"
#include "ConnectionPool.h"
#include "RedisConn.h"
#include "mysqlConn.h"
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <cerrno>
#include <thread>
#include <functional>
#include <ctime>
#include <random>
//工具函数区

std::string readFile(const std::string& path){
    std::string filename = "static" + path;
    std::ifstream file(filename);
    if(!file.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

std::string hashPassword(const std::string& password){
    std::hash<std::string> hasher;
    size_t hash_val = hasher(password);
    return std::to_string(hash_val);
}

static inline int hex2int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1; // 非法
}

std::string urlDecode(const std::string& str) {
    std::string res;
    res.reserve(str.size()); // 预分配，优化
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int h1 = hex2int(str[i+1]);
            int h2 = hex2int(str[i+2]);
            if (h1 != -1 && h2 != -1) {
                res += static_cast<char>(h1 * 16 + h2);
                i += 2;
                continue;
            }
            // 非法十六进制，原样保留%
        }
        else if (str[i] == '+') {
            res += ' ';
            continue;
        }
        res += str[i];
    }
    return res;
}


std::string getParam(const std::string& body,const std::string& key){
    std::string search_key = key + "=";
    size_t pos = 0;
    while(pos < body.size()){
        size_t start = body.find(search_key);
        if(start == std::string::npos) return "";

        if (start == 0 || body[start - 1] == '&'){
            start += search_key.length();
            size_t end = body.find("&",start);

            std::string raw_value;
            if(end == std::string::npos){
                raw_value =  body.substr(start);
            }else{
                raw_value = body.substr(start,end - start);
            } 
            return urlDecode(raw_value);
        }
        pos = start + search_key.size();
    }
    return "";
}

std::string generateToken() {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    std::string token = "sess_";
    for (int i = 0; i < 32; ++i) {
        token += alphanum[dis(gen)];
    }
    return token;
}

std::string TcpConnection::getCookieValue(const std::string& cookie_str, const std::string& key) {
    std::string search_key = key + "=";
    size_t start = cookie_str.find(search_key);
    if (start == std::string::npos) return "";
    
    start += search_key.length();
    size_t end = cookie_str.find(";", start);
    if (end == std::string::npos) {
        return cookie_str.substr(start);
    }
    return cookie_str.substr(start, end - start);
}

//TcpConnection类实现

TcpConnection::TcpConnection(EventLoop* loop, int fd)
    :  id_(0),loop_(loop), fd_(fd), channel_(nullptr), closed_(false) {
    
    last_active_time_ = time(nullptr);

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

std::string TcpConnection::buildResponse(const std::string& body,const std::string& status_line,
                                            const std::string& extra_headers)
{
    return status_line +
           "Content-Type: text/html\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n"
           "Connection: keep-alive\r\n" +
           extra_headers +
           "\r\n" + body;
}

//业务处理区

std::string TcpConnection::handleLogin(const std::string& req_body, std::string& extra_headers) {
    std::string username = getParam(req_body,"username");
    std::string password = getParam(req_body,"password");
    std::string hashed_pwd = hashPassword(password); 
    
    std::cout << "解析出 -> 账号：" << username << ", 密码：" << password << std::endl;

    mysqlConn* conn = ConnectionPool::getInstance().getConnection();
    if(conn == nullptr){
        return "<h1>Server is shutting down. Please try again later.</h1>";
    }

    char safe_username[512] = {0};
    char safe_password[512] = {0};
    mysql_real_escape_string(conn->getRawConn(), safe_username, username.c_str(), username.length());
    mysql_real_escape_string(conn->getRawConn(), safe_password, hashed_pwd.c_str(), hashed_pwd.length());

    std::string sql = "select * from sys_user where username='"+ std::string(safe_username) + "' and password='" + std::string(safe_password) + "'";
    std::cout << "【安全拼接的 SQL】: [" << sql << "]" << std::endl;

    MYSQL_RES* result = conn->query(sql);
    std::string res_body;
    if(result != nullptr && mysql_num_rows(result)>0){
        res_body = "<h1>Login Success! Welcome " + username + "</h1>";
        std::cout << "用户 " << username << " 登录成功！" << std::endl;

        try{
            std::string token = generateToken();
            RedisConn redis;
            if(redis.setex(token, 1800, username)){
                std::cout << "【Redis】生成Session成功, Token: " <<token<<std::endl;
                extra_headers = "Set-Cookie: session_id=" + token + "; Path=/; HttpOnly; Max-Age=1800; SameSite=Lax\r\n";
            }
        }catch(const std::exception& e){
            std::cerr << "Redis 写入失败：" << e.what() << std::endl;
        }
    }else{
        res_body = "<h1>Login Failed! Wrong username or password.</h1>";
        std::cout << "用户 " << username << " 登录失败！" << std::endl;
    }
    
    if (result != nullptr) mysql_free_result(result);
    ConnectionPool::getInstance().returnConnection(conn);
    return res_body;
}

std::string TcpConnection::handleRegister(const std::string& req_body) {
    std::string username = getParam(req_body, "username");
    std::string password = getParam(req_body, "password");
    std::string hashed_pwd = hashPassword(password);
    
    if (username.empty() || password.empty()) {
        return "<h1>Register Failed! Empty username or password.</h1>";
    }

    mysqlConn* conn = ConnectionPool::getInstance().getConnection();
    if(conn == nullptr){
        return "<h1>Server is shutting down. Please try again later.</h1>";
    }

    char safe_username[512] = {0};
    char safe_password[512] = {0};
    mysql_real_escape_string(conn->getRawConn(), safe_username, username.c_str(), username.length());
    mysql_real_escape_string(conn->getRawConn(), safe_password, hashed_pwd.c_str(), hashed_pwd.length());
    
    std::string sql = "INSERT INTO sys_user (username, password) VALUES ('" 
                    + std::string(safe_username) + "', '" + std::string(safe_password) + "')";
    std::cout << "【注册 SQL】: " << sql << std::endl;
    
    std::string res_body;
    if (conn->execute(sql)) {
        res_body = "<h1>Register Success! You can go to login.</h1>";
    } else {
        unsigned int err_code = mysql_errno(conn->getRawConn());
        if (err_code == 1062) {
            res_body = "<h1>Register Failed! Username already exists.</h1>";
            std::cout << "注册失败：用户名 " << username << " 已存在！" << std::endl;
        } else {
            res_body = "<h1>Register Failed! System error.</h1>";
            std::cout << "注册失败：系统错误，错误码 " << err_code << std::endl;
        }
    }
    
    ConnectionPool::getInstance().returnConnection(conn);
    return res_body;
}

std::string TcpConnection::handleProfile(const std::string& cookie_str) {
    std::string token = getCookieValue(cookie_str, "session_id");
    if (token.empty()) {
        return "<h1>401 Unauthorized: No cookie found. Please login.</h1>";
    }
    
    try {
        RedisConn redis;
        std::string username = redis.get(token);
        if (!username.empty()) {
            return "<h1>Welcome to your profile, " + username + "!</h1>";
        } else {
            return "<h1>401 Unauthorized: Session expired or invalid.</h1>";
        }
    } catch (...) {
        return "<h1>500 Server Error: Redis is down.</h1>";
    }
}

std::string TcpConnection::handleGetFile(const std::string& path) {
    std::string req_path = path;
    if(req_path == "/") req_path = "/index.html";
    
    return readFile(req_path);
}

//网络核心区

void TcpConnection::handleRead(){
    if(closed_)  return;

    last_active_time_ = time(nullptr);
    
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
            read_buffer_.append(buffer,valread);

            size_t pos = read_buffer_.find("\r\n\r\n");
            if(pos == std::string::npos) continue;

            int content_length = 0;
            size_t cl_pos = read_buffer_.find("Content-Length: ");
            if(cl_pos != std::string::npos && cl_pos < pos){
                size_t end_pos = read_buffer_.find("\r\n",cl_pos);
                std::string cl_str = read_buffer_.substr(cl_pos+16, end_pos - (cl_pos + 16));
                content_length = std::stoi(cl_str);
            } 
            size_t total_len = pos + 4 +content_length;
            if(read_buffer_.size() < total_len) continue;

            std::string complete_request = read_buffer_.substr(0,total_len);
            read_buffer_.erase(0, total_len);

            char method[16], path[256], version[16];
            sscanf(complete_request.c_str(), "%s %s %s", method, path, version);
            std::string status_line = "HTTP/1.1 200 OK\r\n";
            std::string res_body;
            std::string extra_headers = "";

            if(strcmp(method,"POST")==0){
                std::string req_body = complete_request.substr(pos+4);

                std::cout << "【收到 POST 请求】路径: " << path << ", Body: " << req_body << std::endl;

                if(strcmp(path,"/login")==0){
                    res_body = handleLogin(req_body, extra_headers);                                               
                }else if (strcmp(path, "/register") == 0) {
                    res_body = handleRegister(req_body);
                }else{
                    res_body = "<h1>Invalid API</h1>";
                    status_line = "HTTP/1.1 400 Bad Request\r\n";
                }
            }else{
                std::cout<< "path: " << path << std::endl;
                if (strcmp(path, "/profile") == 0) {
                    std::string req_header = complete_request.substr(0, pos);
                    size_t cookie_pos = req_header.find("Cookie: ");
                    std::string cookie_str = "";
                    
                    if (cookie_pos != std::string::npos) {
                        size_t end_pos = req_header.find("\r\n", cookie_pos);
                        cookie_str = req_header.substr(cookie_pos + 8, end_pos - (cookie_pos + 8));
                    }
                    
                    res_body = handleProfile(cookie_str);
                    if (res_body.find("401") != std::string::npos) {
                        status_line = "HTTP/1.1 401 Unauthorized\r\n";
                    }
                }else{
                    res_body = handleGetFile(path);
                    if(res_body.empty()){
                        res_body = "<h1>404 Not Found</h1>";
                        status_line = "HTTP/1.1 404 Not Found\r\n";
                    }
                } 
            }

            std::string response = buildResponse(res_body, status_line, extra_headers);

            size_t total_sent = 0;
                while(total_sent < response.size()){
                    ssize_t n = ::send(fd_, response.c_str() + total_sent, response.size() - total_sent,0);
                    if(n<=0){
                        if(errno == EINTR) continue;
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            continue;
                        }
                        handleClose();
                        return;
                    }
                    total_sent += n;
                }
                break;
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