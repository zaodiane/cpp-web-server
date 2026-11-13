#pragma once

#include<mysql/mysql.h>
#include<string>
#include<stdexcept>

class mysqlConn{
    public:
        mysqlConn(const std::string& host, const std::string& user,
                const std::string& password, const std::string& db,
                unsigned int port = 3306);
        
        ~mysqlConn();

        mysqlConn(const mysqlConn&) = delete;
        mysqlConn& operator=(const mysqlConn&) = delete;

        bool execute(const std::string& sql);

        MYSQL_RES* query(const std::string& sql);

        MYSQL* getRawConn() const { return conn_; }
    private:
        MYSQL *conn_;
};