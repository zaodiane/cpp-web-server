#include "TcpServer.h"
#include "ConnectionPool.h"
#include <iostream>

int main() {
    try {
        ConnectionPool::getInstance().init("localhost", "root", "070814", "practice", 5);
        std::cout << "数据库连接池初始化成功！" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "连接池初始化失败: " << e.what() << std::endl;
        return -1; 
    }
    EventLoop mainLoop;

    TcpServer server(&mainLoop, 8080, 4);

    server.start();

    mainLoop.loop();

    return 0;
}