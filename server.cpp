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
#define UDP_SIZE 1500
#define UDP_DATA_SIZE (UDP_SIZE - 4)
#define MAX_SEQ_NUM (MAIN_BUF_SIZE - (MAIN_BUF_SIZE % UDP_DATA_SIZE))/UDP_DATA_SIZE
#define localhost "127.0.0.1"

using namespace std;

int sockfd;
int itr_done;
unordered_set <int> remaining_packet_hash;
pthread_mutex_t hash_mutex;
pthread_mutex_t itr_done_mutex;
pthread_mutex_t mainBuf_mutex;

char * mainBuf;

// UDP Socket setup code from Beej’s Guide to Network Programming
int SetupUDPSocket(const char *port)
{
    struct addrinfo hints;
    struct addrinfo *res;
    struct timeval read_timeout;
    read_timeout.tv_sec = 1;
    read_timeout.tv_usec = 0;
    int status;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints); // make sure hints is empty
    hints.ai_family = AF_INET;       // use IPv4
    hints.ai_socktype = SOCK_DGRAM;  // use datagram sockets

    // error checking for getaddrinfo

    if ((status = getaddrinfo("10.0.1.170", port, &hints, &res)) != 0)
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
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));

    freeaddrinfo(res); // all done with this structure
    return sockfd;
}

void *send_thread(void* serverinfo) {

    printf("send thread started\n");
    struct addrinfo *servinfo = (struct addrinfo *) serverinfo;
	
    int hash_size = MAX_SEQ_NUM;
    int local_itr_done;
    while (hash_size) {
 
        pthread_mutex_lock(&itr_done_mutex);
        local_itr_done = itr_done;
        pthread_mutex_unlock(&itr_done_mutex);

        if (local_itr_done) {
            string ack_string = "";
            pthread_mutex_lock(&hash_mutex);
            for (const auto& elem: remaining_packet_hash) {
                ack_string += to_string(elem) + " ";
            }  
	    pthread_mutex_unlock(&hash_mutex);

            printf("Ack packet string %s\n", ack_string.c_str());
            for (int i = 0; i < 5; i++) {

                sendto(sockfd, ack_string.c_str(), ack_string.length(), 0, servinfo->ai_addr, servinfo->ai_addrlen);

                printf("Sent Ack %d\n", i);
            }
            printf("Done sending ack packets\n");
            break;
        }
        
        // Update hash size
        pthread_mutex_lock(&hash_mutex);
        hash_size = remaining_packet_hash.size();
        pthread_mutex_unlock(&hash_mutex);

    }
    

}

void *recv_thread(void* clientinfo) {
    
    printf("recv thread started\n");
    int queue_empty = 1;
    int numbytes;
    struct sockaddr_in cli_info = *(struct sockaddr_in *) clientinfo;
    socklen_t addr_len = sizeof(cli_info);

    char localBuf[UDP_SIZE]; 
    int hash_size = MAX_SEQ_NUM;

    while(hash_size) {
        

        numbytes = recvfrom(sockfd, (char *)localBuf, UDP_SIZE, 0, (struct sockaddr *) &cli_info, &addr_len);

        if (numbytes < 0) {
            
            // Timeout has occured. Update hash size and restart loop.
            pthread_mutex_lock(&hash_mutex);
            hash_size = remaining_packet_hash.size();
            pthread_mutex_unlock(&hash_mutex);
            printf("timeout has occurred\n");
            printf("Packets recvd %d\n", (MAX_SEQ_NUM - hash_size));
            continue;
        }


        int seq_num = (localBuf[0] << 8) | localBuf[1] & 0xFF;
        int packet_itr_done = (localBuf[2]) & 0x02;
        int packet_full_done = (localBuf[2]) & 0x01;

        printf("Received seq num: %d\n", seq_num);

        if (packet_itr_done) {
            // Done packet received. Set itr_done flag.
            pthread_mutex_lock(&itr_done_mutex);
            itr_done = 1;
            pthread_mutex_unlock(&itr_done_mutex);
        }
        else {
            // Data packet received. Store data in buffer
            pthread_mutex_lock(&mainBuf_mutex);
            memcpy(&mainBuf[seq_num*UDP_DATA_SIZE], &localBuf[4], UDP_DATA_SIZE);
            pthread_mutex_unlock(&mainBuf_mutex);

            // Update hash
            pthread_mutex_lock(&hash_mutex);
            remaining_packet_hash.erase(seq_num);
            hash_size = remaining_packet_hash.size();
            pthread_mutex_unlock(&hash_mutex);
        }

    }

    
}

// Driver code
int main()
{

    sockfd = SetupUDPSocket(SERVER_PORT);
    struct sockaddr_in cliaddr;
    socklen_t addr_len = sizeof(cliaddr);
    int numbytes;

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



    for (int i = 0; i < MAX_SEQ_NUM; i++ ) {
        remaining_packet_hash.insert(i);
    }

    mainBuf = new char[MAIN_BUF_SIZE];
    pthread_t tid[6];
    for (int i = 0; i < 5; i++) {
        pthread_create(&tid[i], NULL, recv_thread, (void*) &cliaddr);
    }
    pthread_create(&tid[5], NULL, send_thread, (void*) servinfo);
    
    for (int i = 0; i < 6; i++) {
        pthread_join(tid[i], NULL);
    }
    close(sockfd);

    return 0;
}
