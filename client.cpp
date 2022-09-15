// Client side implementation of UDP client-server model
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <string>
#include <pthread.h>
#include <iostream>
#include <optional>
#include "safequeue.h"

#define CLIENT_PORT "25581"
#define SERVER_PORT "25580"
#define localhost "127.0.0.1"
#define MAXLINE 1024
#define FILE_SIZE 16777216
#define UDP_SIZE 65507
#define UDP_DATA_SIZE UDP_SIZE - 2

using namespace std;

int sock_fd = 0;
ThreadsafeQueue<int> send_queue;
pthread_mutex_t mt = PTHREAD_MUTEX_INITIALIZER;

// UDP Socket setup code from Beej’s Guide to Network Programming
int SetupUDPSocket(const char *port)
{
    struct addrinfo hints;
    struct addrinfo *res;
    int sockfd;
    int status;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints); // make sure hints is empty
    hints.ai_family = AF_INET;       // use IPv4
    hints.ai_socktype = SOCK_DGRAM;  // use datagram sockets

    // error checking for getaddrinfo

    if ((status = getaddrinfo(localhost, port, &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // make a socket:

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    // error checking for socket creation

    if (sockfd == -1)
    {
        perror("Central UDP: socket");
        exit(1);
    }

    // bind it to the port and IP address we passed in to getaddrinfo():

    if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
        close(sockfd);
        perror("Central UDP: bind");
        exit(1);
    }

    freeaddrinfo(res); // all done with this structure
    return sockfd;
}

int ReadQueue()
{
    optional<int> num = send_queue.pop();
    if(!num.has_value())
        return 0;
    else
        return num.value();
}

// Client sends to server
void *ClientSendTo(void *serverinfo)
{

    int numbytes;
    struct addrinfo *servinfo = (struct addrinfo *)serverinfo;
    string hello = "Hello from client";
    // if(seq_no== 107)
    // sendto(sock_fd, FileChunk(seq_no))
    if ((numbytes = sendto(sock_fd, hello.data(), hello.length(), 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
    {
        perror("Central: ServerP sendto num nodes");
        exit(1);
    }

    printf("Hello message sent.\n");
}

// Driver code
int main()
{
    sock_fd = SetupUDPSocket(CLIENT_PORT);

    struct addrinfo hints;
    struct addrinfo *servinfo;
    int status;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints); // make sure hints is empty
    hints.ai_family = AF_INET;       // use IPv4
    hints.ai_socktype = SOCK_DGRAM;  // use datagram sockets

    // error checking for getaddrinfo
    // getaddrinfo used to get server address

    if ((status = getaddrinfo(localhost, SERVER_PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // Initialize queue
    int file_last_index = (FILE_SIZE-1)/UDP_DATA_SIZE;

    for(int i = 0; i<=file_last_index; i++)
    {
        send_queue.push(i);
    }

    pthread_t tid[5];
    for (int i = 0; i < 5; i++)
    {
        pthread_create(&tid[i], NULL, ClientSendTo, (void*)servinfo);
    }
    for (int i = 0; i < 5; i++)
    {
        pthread_join(tid[i],NULL);
    }

    close(sock_fd);
    return 0;
}