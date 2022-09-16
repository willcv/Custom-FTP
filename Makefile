# the compiler: gcc for C program, define as g++ for C++
CC = g++-7

all: server.out client.out

server.out: server.cpp
	@ $(CC) -std=c++17 -o server server.cpp

client.out: client.cpp
	@ $(CC) -std=c++17 -o client client.cpp -lpthread

clean:
	@ $(RM) server client
