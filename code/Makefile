CXX = g++
CXXFLAGS = -std=c++11 -Wall -g
TARGET = ../../bin/reactor_v2
SRCS = src/EventLoop.cpp src/Channel.cpp src/Acceptor.cpp src/TcpConnection.cpp src/TcpServer.cpp src/ConnectionPool.cpp src/mysqlConn.cpp src/RedisConn.cpp src/main.cpp
INCLUDES = -Iinclude

LDFLAGS = -lmysqlclient -lhiredis -pthread 

server: $(SRCS)
	mkdir -p ../../bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $^  $(LDFLAGS)

clean:
	rm -f $(TARGET)
