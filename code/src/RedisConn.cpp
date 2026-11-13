#include "RedisConn.h"
#include <iostream>

RedisConn::RedisConn(const std::string& host, int port){
    context_ = redisConnect(host.c_str(),port);
    if(context_ == nullptr || context_->err){
        if(context_) redisFree(context_);
        throw std::runtime_error("Redis Connect Error");
    }
}

RedisConn::~RedisConn(){
    if(context_) redisFree(context_);
}

bool RedisConn::setex(const std::string& key, int seconds, const std::string& value){
    redisReply* reply = (redisReply*)redisCommand(context_, "setex %s %d %s", key.c_str(), seconds, value.c_str());
    if(!reply) return false;
    bool success = (reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK");
    freeReplyObject(reply);
    return success;
}

std::string RedisConn::get(const std::string& key){
    redisReply* reply = (redisReply*)redisCommand(context_, "get %s",key.c_str());
    if(!reply) return "";
    std::string value = "";
    if(reply->type == REDIS_REPLY_STRING){
        value = std::string(reply->str, reply->len);
    }
    freeReplyObject(reply);
    return value;
}
