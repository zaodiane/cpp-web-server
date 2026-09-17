CXX = g++
CXXFLAGS = -std=c++11 -Wall -g -pthread
TARGET = ../../bin/reactor
SRCS = src/EventLoop.cpp src/Channel.cpp src/Acceptor.cpp src/TcpConnection.cpp src/TcpServer.cpp src/main.cpp
INCLUDES = -Iinclude

server: $(SRCS)
	mkdir -p ../../bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(TARGET) $^

clean:
	rm -f $(TARGET)
