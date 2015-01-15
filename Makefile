CFLAGS_L=-Wall -g -lcurl -lboost_regex -lboost_thread
CFLAGS=-W -Wall -g -std=c++11 -Wextra
CFLAGS_PRODUCTION=-s -O2 -flto -march=native

all: ruopen
.PHONY: all
.PHONY: clean

ruopen: utils.o ruopen.o
	g++ $(CFLAGS_L) -o ruopen ruopen.o utils.o -ljsoncpp

ruopen.o: ruopen.cpp ruopen.h
	g++ $(CFLAGS) -c ruopen.cpp

utils.o: utils.cpp
	g++ $(CFLAGS) -c utils.cpp

clean:
	rm -f ruopen *.o response.html cookiejar.txt
