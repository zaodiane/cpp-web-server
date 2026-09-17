#include "TcpServer.h"
#include <iostream>

int main() {
    EventLoop mainLoop;

    TcpServer server(&mainLoop, 8080, 4);

    server.start();

    mainLoop.loop();

    return 0;
}