PROGRAM=ruopen
CC=g++
CXXFLAGS_DEBUG=-std=c++11 -Wall -Wextra -g -Og
CXXFLAGS=-std=c++11 -s -O2 -flto -march=native
LDFLAGS=-lcurl -lboost_regex -lboost_thread -lboost_system -ljsoncpp

all: $(PROGRAM)
.PHONY: all
.PHONY: clean

$(PROGRAM): utils.o $(PROGRAM).o
	$(CC) -o $(PROGRAM) $(PROGRAM).o utils.o $(LDFLAGS)

$(PROGRAM).o: $(PROGRAM).cpp $(PROGRAM).h
	$(CC) $(CXXFLAGS) -c $(PROGRAM).cpp

utils.o: utils.cpp
	$(CC) $(CXXFLAGS) -c utils.cpp

clean:
	rm -f $(PROGRAM) *.o response.html cookiejar.txt
