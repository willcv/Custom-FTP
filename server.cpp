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

#define THREAD_NUM 5
#define RECV_T1_PORT "25581"
#define RECV_T2_PORT "25582"
#define RECV_T3_PORT "25583"
#define RECV_T4_PORT "25584"
#define RECV_T5_PORT "25585"
#define SEND_PORT "25580"

#define MAIN_BUF_SIZE 65536*16384
#define UDP_SIZE 32768
#define HEADER_SIZE 5
#define UDP_DATA_SIZE (UDP_SIZE - HEADER_SIZE)
#define ACK_HEADER_SIZE 1
#define UDP_ACK_SIZE (UDP_SIZE - ACK_HEADER_SIZE)
#define MAX_SEQ_NUM (MAIN_BUF_SIZE - (MAIN_BUF_SIZE % UDP_DATA_SIZE))/UDP_DATA_SIZE
#define localhost "127.0.0.1"

using namespace std;

int sockfd[5];
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
    //struct addrinfo *res;
    struct timeval read_timeout;
    read_timeout.tv_sec = 1;
    read_timeout.tv_usec = 0;
    int status;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints); // make sure hints is empty
    hints.ai_family = AF_INET;       // use IPv4
    hints.ai_socktype = SOCK_DGRAM;  // use datagram sockets

    // error checking for getaddrinfo

    if ((status = getaddrinfo("10.0.2.66", port, &hints, &res)) != 0)
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
    char localBuf[UDP_SIZE];
    while (hash_size) {
 
        pthread_mutex_lock(&itr_done_mutex);
        local_itr_done = itr_done;
        itr_done = 0;
        pthread_mutex_unlock(&itr_done_mutex);

        if (local_itr_done) {
            string ack_string = "";
            pthread_mutex_lock(&hash_mutex);
            int * seq_no_arr = int[remaining_packet_hash.size()];
            int i = 0;
            int remaing_acks_to_send = remaining_packet_hash.size();
            for (const auto& elem: remaining_packet_hash) {
                seq_no_arr[i] = elem;
                i++; 
            }  
	    int acks_sent = 0;
            while(remaining_acks_to_send) {
                int packet_ack_num = min(UDP_ACK_SIZE, remaining_acks_to_send);
                memcpy(&localBuf[1], &seq_no_arr[acks_sent],  sizeof(int) * packet_ack_num);
                acks_sent += packet_ack_num;
                remaining_acks_to_send -= packet_ack_num;

                localBuf[0] = remaining_ack_to_send == 0 ? 1 : 0;
                printf("Sending %d Acks\n", packet_ack_num);
                for (int i = 0; i < 5; i++) {

                    sendto(sockfd, &localBuf, sizeof(int)*packet_ack_num, 0, servinfo->ai_addr, servinfo->ai_addrlen);

                    printf("Sent Ack %d\n", i);
                }
            } 

               
            printf("Done sending ack packets\n");
        
        
	    pthread_mutex_unlock(&hash_mutex);
        }
        // Update hash size
        pthread_mutex_lock(&hash_mutex);
        hash_size = remaining_packet_hash.size();
        pthread_mutex_unlock(&hash_mutex);

    }
    

}

void *recv_thread(void* sockfd) {
    
    printf("recv thread started\n");
    int queue_empty = 1;
    int numbytes;
    int thread_sockfd = (int) sockfd;
    struct sockaddr_in cli_info;
    socklen_t addr_len = sizeof(cli_info);

    char localBuf[UDP_SIZE]; 
    int hash_size = MAX_SEQ_NUM;

    while(hash_size) {
        
        numbytes = recvfrom(thread_sockfd, (char *)localBuf, UDP_SIZE, 0, (struct sockaddr *) &cli_info, &addr_len);

        if (numbytes < 0) {
            
            // Timeout has occured. Update hash size and restart loop.
            pthread_mutex_lock(&hash_mutex);
            hash_size = remaining_packet_hash.size();
            pthread_mutex_unlock(&hash_mutex);
            printf("timeout has occurred\n");
            printf("Packets recvd %d\n", (MAX_SEQ_NUM - hash_size));
            continue;
        }
   
        int seq_num;
        memcpy(&seq_num, &localBuf, sizeof(seq_num));

        //iint seq_num = (localBuf[0] << 8) | localBuf[1] & 0xFF;
        //int packet_itr_done = (localBuf[2]) & 0x02;
        int packet_full_done = (localBuf[4]) & 0x01;

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

    //struct addrinfo*5];
    // Set up receiving sockets

    sockfd[0] = SetupUDPSocket(RECV_T1_PORT);
    sockfd[1] = SetupUDPSocket(RECV_T2_PORT);
    sockfd[2] = SetupUDPSocket(RECV_T3_PORT);
    sockfd[3] = SetupUDPSocket(RECV_T4_PORT);
    sockfd[4] = SetupUDPSocket(RECV_T5_PORT);

    // Set up sending socket   

    struct addrinfo hints;
    struct addrinfo *servinfo;
    int status;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints); // make sure hints is empty
    hints.ai_family = AF_INET;       // use IPv4
    hints.ai_socktype = SOCK_DGRAM;  // use datagram sockets

    // error checking for getaddrinfo
    // getaddrinfo used to get server address

    if ((status = getaddrinfo("10.0.1.115", SEND_PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }



 
    for (int i = 0; i < MAX_SEQ_NUM; i++ ) {
        remaining_packet_hash.insert(i);
    }

    mainBuf = new char[MAIN_BUF_SIZE];
    pthread_t tid[11];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&tid[i], NULL, recv_thread, (void*) sockfd[i]);
    }
    pthread_create(&tid[NUM_THREADS], NULL, send_thread, (void*) servinfo);
    
    for (int i = 0; i < NUM_THREADS+1; i++) {
        pthread_join(tid[i], NULL);
    }
    close(sockfd);

    auto myfile = std::fstream("file.out", std::ios::out | std::ios::binary);
    myfile.write((char*)&mainBuf[0], MAIN_BUF_SIZE);
    myfile.close();
    return 0;
}
