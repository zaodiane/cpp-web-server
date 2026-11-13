#include "mysqlConn.h"
#include <iostream>

mysqlConn::mysqlConn(const std::string& host, const std::string& user,
                const std::string& password, const std::string& db,
                unsigned int port)
{
    conn_ = mysql_init(NULL);
    if(conn_ == NULL){
        throw std::runtime_error("init failed");
    }

    if(mysql_real_connect(conn_, host.c_str(), user.c_str(), password.c_str(), db.c_str(), port, NULL, 0)==NULL){
        std::string err_msg = "MySQL connect failed: " + std::string(mysql_error(conn_));
        mysql_close(conn_);
        throw std::runtime_error(err_msg);
    }
    std::cout << "MYSQL 连接成功" << std::endl;
}

mysqlConn::~mysqlConn(){
    if(conn_){
        mysql_close(conn_);
        conn_ = nullptr;
    }
}

bool mysqlConn::execute(const std::string& sql) {
    if (mysql_query(conn_, sql.c_str()) != 0) {
        std::cerr << "执行失败: " << mysql_error(conn_) << std::endl;
        return false;
    }
    return true;
}

MYSQL_RES* mysqlConn::query(const std::string& sql) {
    if (mysql_query(conn_, sql.c_str()) != 0) {
        std::cerr << "查询失败: " << mysql_error(conn_) << std::endl;
        return nullptr;
    }

    MYSQL_RES* result = mysql_store_result(conn_);
    if (result == nullptr) {
        if (mysql_field_count(conn_) == 0) {
            return nullptr; 
        }
        std::cerr << "获取结果集失败: " << mysql_error(conn_) << std::endl;
        return nullptr;
    }
    
    return result;
}