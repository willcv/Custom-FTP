# the compiler: gcc for C program, define as g++ for C++
CC = g++-7

all: server.out client.out

server.out: server.cpp
	@ $(CC) -o server server.cpp -lpthread

client.out: client.cpp
	@ $(CC) -o client client.cpp -lpthread

clean:
	@ $(RM) server client
