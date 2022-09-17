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
#include <time.h>
#include "safequeue.h"
#include "common.h"

using namespace std;

ThreadsafeQueue<int> send_queue;
char *main_buf;

// UDP Socket setup code from Beej’s Guide to Network Programming
int SetupUDPSocket(const char *ip, const char *port)
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

    if ((status = getaddrinfo(ip, port, &hints, &res)) != 0)
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

void GetUDPServerInfo(const char *ip, const char *port, struct addrinfo *&servinfo)
{

    struct addrinfo hints;
    int status;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints); // make sure hints is empty
    hints.ai_family = AF_INET;       // use IPv4
    hints.ai_socktype = SOCK_DGRAM;  // use datagram sockets

    // error checking for getaddrinfo
    // getaddrinfo used to get server address

    if ((status = getaddrinfo(ip, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
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
void *ClientSendTo(void *arg)
{
    int thread_idx = (intptr_t)arg;
    int sock_fd = SetupUDPSocket(CLIENT_IP, CLIENT_THREAD_PORTS[thread_idx]);
    char small_buf[UDP_SIZE];
    int numbytes;
    struct addrinfo *servinfo;
    GetUDPServerInfo(SERVER_IP, SERVER_THREAD_PORTS[thread_idx], servinfo);
    int sequence_num;
    while ((sequence_num = ReadQueue()) != -1)
    {
        memcpy(&small_buf[5], &main_buf[sequence_num * UDP_DATA_SIZE], UDP_DATA_SIZE);
        memcpy(&small_buf, &sequence_num, sizeof(sequence_num));
        small_buf[4] = 0;
        if ((numbytes = sendto(sock_fd, small_buf, UDP_SIZE, 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
        {
            perror("Central: ServerP sendto num nodes");
            exit(1);
        }
        printf("sent %d\n", sequence_num);
    }

    printf("Hello message sent.\n");
}

// Driver code
int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Invalid command\n");
        printf("Usage: ./client [user@]SRC_HOST:]file1 [user@]DEST_HOST:]file2\n");
    }

    int sock_fd = SetupUDPSocket(CLIENT_IP, CLIENT_MAIN_PORT);

    // Read file and initialize main buffer
    ifstream input_file(argv[1]);
    main_buf = new char[FILE_SIZE];

    input_file.read(main_buf, FILE_SIZE);

    // Initialize queue
    int file_last_index = (FILE_SIZE - 1) / UDP_DATA_SIZE;

    for (int i = 0; i <= file_last_index; i++)
    {
        send_queue.push(i);
    }

    // Begin measuring time
    time_t begin, end;
    time(&begin);

    pthread_t tid[5];
    for (int i = 0; i < 3; i++)
    {
        pthread_create(&tid[i], NULL, ClientSendTo, (void *)(intptr_t)i);
    }
    for (int i = 0; i < 1; i++)
    {
        pthread_join(tid[i], NULL);
    }

    // Stop measuring time and calculate the elapsed time
    time(&end);
    time_t elapsed = end - begin;
    printf("Time measured: %ld seconds.\n", elapsed);

    close(sock_fd);
    return 0;
}
