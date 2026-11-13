#pragma once
#include <hiredis/hiredis.h>
#include <string>
#include <stdexcept>

class RedisConn{
    public:
        RedisConn(const std::string& host = "127.0.0.1", int port = 6379);
        ~RedisConn();

        bool setex(const std::string& key, int seconds, const std::string& value);
        std::string get(const std::string& key);

        RedisConn(const RedisConn&) = delete;
        RedisConn& operator=(const RedisConn&) = delete;
    private:
        redisContext* context_;
};