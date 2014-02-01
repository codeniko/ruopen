CFLAGS_L=-Wall -g -lcurl -ljson -lboost_regex -lboost_thread
CFLAGS=-W -Wall -g -std=c++11 -Wextra -pedantic
CFLAGS_PRODUCTION=-s -O2 -flto -march=native

all: ruopen
.PHONY: all
.PHONY: clean

ruopen: ruopen.o
	g++ $(CFLAGS_L) -o ruopen ruopen.o

ruopen.o: ruopen.cpp utils.cpp
	g++ $(CFLAGS) -c ruopen.cpp 

clean:
	rm -f ruopen *.o response.html cookiejar.txt
