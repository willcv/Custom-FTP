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
#include <fstream>
#include "safequeue.h"

#define CLIENT_PORT "25581"
#define SERVER_PORT "25580"
#define localhost "127.0.0.1"
#define MAXLINE 1024
#define FILE_SIZE 16*1024*1024
#define UDP_SIZE 1500
#define UDP_DATA_SIZE UDP_SIZE - 4

using namespace std;

int sock_fd = 0;
ThreadsafeQueue<int> send_queue;
pthread_mutex_t mt = PTHREAD_MUTEX_INITIALIZER;
char* main_buf;

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

    if ((status = getaddrinfo("10.0.2.240", port, &hints, &res)) != 0)
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
    if (!num.has_value())
        return -1;
    else
        return num.value();
}

char *FileMap(int sequence_num, char *main_buffer)
{
    return &main_buffer[sequence_num * UDP_DATA_SIZE];
}

// Client sends to server
void *ClientSendTo(void *serverinfo)
{
    char small_buf[UDP_SIZE];
    int numbytes;
    struct addrinfo *servinfo = (struct addrinfo *)serverinfo;
    int sequence_num;
    while ((sequence_num = ReadQueue()) != -1)
    {
        memcpy(&small_buf[4], &main_buf[sequence_num * UDP_DATA_SIZE], UDP_DATA_SIZE);
        small_buf[0] = (sequence_num >> 8) & 0xFF;
        small_buf[1] = sequence_num & 0xFF;
        if ((numbytes = sendto(sock_fd, small_buf, UDP_SIZE, 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
        {
            perror("Central: ServerP sendto num nodes");
            exit(1);
        }
        printf("sent %d\n", sequence_num);
    }

    // int numbytes;
    // struct addrinfo *servinfo = (struct addrinfo *)serverinfo;
    // int chunk_index;
    // while ((chunk_index = ReadQueue()) != -1)
    // {
    //     string temp = to_string(chunk_index);
    //     if ((numbytes = sendto(sock_fd, temp.data(), temp.length(), 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
    //     {
    //         perror("Central: ServerP sendto num nodes");
    //         exit(1);
    //     }
    // }
    // if(seq_no== 107)
    // sendto(sock_fd, FileChunk(seq_no))

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

    if ((status = getaddrinfo("10.0.1.170", SERVER_PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // Read file and initialize main buffer
    ifstream input_file("data.bin");
    main_buf = new char[FILE_SIZE];

    input_file.read(main_buf, FILE_SIZE);

    // Initialize queue
    int file_last_index = (FILE_SIZE - 1) / UDP_DATA_SIZE;

    for (int i = 0; i <= file_last_index; i++)
    {
        send_queue.push(i);
    }

    pthread_t tid[5];
    for (int i = 0; i < 1; i++)
    {
        pthread_create(&tid[i], NULL, ClientSendTo, (void *)servinfo);
    }
    for (int i = 0; i < 1; i++)
    {
        pthread_join(tid[i], NULL);
    }

    close(sock_fd);
    return 0;
}
