CXX =	g++
CFLAGS = -std=c++11 -O2 -Wall -pthread

TARGET = server
OBJS = src/main.cpp src/WebServer.cpp src/HttpConn.cpp src/Epoller.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET)

clean:
	rm -f $(TARGET)