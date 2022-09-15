// Server side implementation of UDP client-server model
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
#include <unordered_set>

#define CLIENT_PORT "25581"
#define SERVER_PORT "25580"
#define MAIN_BUF_SIZE 65536*16384
#define UDP_SIZE 65507
#define UDP_DATA_SIZE UDP_SIZE - 4

#define localhost "127.0.0.1"

using namespace std;

int sockfd;
int itr_done;
unordered_set <int> neg_acks;


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

void *send_thread(void*) {


}

void *recv_thread(void*) {
    
    int queue_empty = 1;
    while(!queue_empty) {
        
       

    }
}

// Driver code
int main()
{
    sockfd = SetupUDPSocket(SERVER_PORT);
    struct sockaddr_in cliaddr;
    socklen_t addr_len = sizeof(cliaddr);
    int numbytes;

    char buffer[MAXLINE];

    while (1)
    {
        if ((numbytes = recvfrom(sockfd, (char *)buffer, MAXLINE, 0, (struct sockaddr *)&cliaddr, &addr_len)) == -1)
        {
            perror("ServerP: Central recvfrom scores list");
            exit(1);
        }

        buffer[numbytes] = '\0';
        printf("Client : %s\n", buffer);
    }

    close(sockfd);

    return 0;
:}
