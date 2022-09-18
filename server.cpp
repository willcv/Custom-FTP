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
#include <fstream>
#include <chrono>
#include <iostream>

#define NUM_THREADS 4
#define RECV_T1_PORT "25581"
#define RECV_T2_PORT "25582"
#define RECV_T3_PORT "25583"
#define RECV_T4_PORT "25584"
#define RECV_T5_PORT "25585"
#define SEND_PORT "25580"

#define MAIN_BUF_SIZE 65536*16384 //16*1024*1024
#define UDP_SIZE 1472
#define HEADER_SIZE 5
#define UDP_DATA_SIZE (UDP_SIZE - HEADER_SIZE)
#define ACK_HEADER_SIZE 4
#define UDP_ACK_SIZE (UDP_SIZE - ACK_HEADER_SIZE)
#define MAX_SEQ_NUM (MAIN_BUF_SIZE - (MAIN_BUF_SIZE % UDP_DATA_SIZE))/UDP_DATA_SIZE
#define DUPLICATE_ACKS 4
#define CLIENT_IP "10.0.1.115"
#define SERVER_IP "10.0.2.66"

using namespace std;
using namespace std::chrono;
int recv_sockfd[5];
int send_sockfd;
int itr_done;
int ack_sent;
unordered_set <int> remaining_packet_hash;
pthread_mutex_t hash_mutex;
pthread_mutex_t itr_done_mutex;
pthread_mutex_t mainBuf_mutex;
pthread_mutex_t ack_sent_mutex;
char * mainBuf;

// UDP Socket setup code from Beej’s Guide to Network Programming
int SetupUDPSocket(const char *port, const char *ip)
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

    if ((status = getaddrinfo(ip, port, &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // make a socket:

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

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
    int local_ack_sent;
    int itr_num = 0;


    int prev_local_ack;
    while (hash_size) {
 
        pthread_mutex_lock(&ack_sent_mutex);
        local_ack_sent = ack_sent;
        pthread_mutex_unlock(&ack_sent_mutex);

        if (prev_local_ack == 1 && local_ack_sent == 0) {
            pthread_mutex_lock(&itr_done_mutex);
            itr_done = 0;
            pthread_mutex_unlock(&itr_done_mutex);
        }
        prev_local_ack = local_ack_sent;

        if (local_ack_sent) {
            continue;
        }
        // Ack is 0. If ack has just been unset, set itr_done to 0
        // else, 
        pthread_mutex_lock(&itr_done_mutex);
        local_itr_done = itr_done;
        pthread_mutex_unlock(&itr_done_mutex);

        if (local_itr_done) {
            string ack_string = "";
            pthread_mutex_lock(&hash_mutex);
            int * seq_no_arr = new int[remaining_packet_hash.size()];
            int i = 0;
            int remaining_acks_to_send = remaining_packet_hash.size();
            //printf("Remaining acks_to_send: %d\n", remaining_acks_to_send);
            //printf("Here 2\n");
            for (const auto& elem: remaining_packet_hash) {
                seq_no_arr[i] = elem;
                i++; 
            }  
            
	    int acks_sent = 0;
            while(remaining_acks_to_send) {


                int packet_ack_num = min(UDP_ACK_SIZE/4, remaining_acks_to_send);
                memcpy(&localBuf[4], &seq_no_arr[acks_sent],  sizeof(int) * packet_ack_num);
                acks_sent += packet_ack_num;
                remaining_acks_to_send -= packet_ack_num;

                localBuf[0] = remaining_acks_to_send == 0 ? 1 : 0;
                // Store 3 bytes of itr_num into local buf
                memcpy(&localBuf[1], &itr_num, 3);
                for (int i = 0; i < DUPLICATE_ACKS; i++) {

                    usleep(600);
                    sendto(send_sockfd, localBuf, sizeof(int)*packet_ack_num+4, 0, servinfo->ai_addr, servinfo->ai_addrlen);

                }
            } 

            //usleep(1000);                 
            itr_num ++;
            printf("Iteration Done\n"); 
	    pthread_mutex_unlock(&hash_mutex);

            pthread_mutex_lock(&ack_sent_mutex);
            ack_sent = 1;
            pthread_mutex_unlock(&ack_sent_mutex);
        }
        // Update hash size
        pthread_mutex_lock(&hash_mutex);
        hash_size = remaining_packet_hash.size();
        pthread_mutex_unlock(&hash_mutex);

    }

    cout << "Ending Timestamp: " << duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() << endl;
    // Sending Final Done Packet
    for (int i = 0; i < 20; i++) {
        char done_buf[4];
        done_buf[0] = 1;
        memcpy(&done_buf[1], &itr_num, 3);
        sendto(send_sockfd, done_buf, 4, 0, servinfo->ai_addr, servinfo->ai_addrlen);

    }
    printf("Sent Final Ack\n");
}

void *recv_thread(void* sockfd) {
    
    printf("recv thread started\n");
    int queue_empty = 1;
    int numbytes;
    int thread_sockfd = *(int*) sockfd;
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
            continue;
        }
   
  //      printf("Here 1\n");
        int seq_num;
        if (numbytes > 10) {
            memcpy(&seq_num, &localBuf, sizeof(seq_num));
            printf("Received seq num: %d\n", seq_num);
        }
        //iint seq_num = (localBuf[0] << 8) | localBuf[1] & 0xFF;
        //int packet_itr_done = (localBuf[2]) & 0x02;
        int packet_itr_done = (localBuf[4]) & 0x01;


//        printf("Here 2\n");
        int continue_flag = 0;
        pthread_mutex_lock(&ack_sent_mutex);
        if (ack_sent) {
            if (!packet_itr_done) {
                ack_sent = 0;
          //      printf("Received data packet, unsetting ack_sent\n");
            } else {
                continue_flag = 1;
            }
        }
        pthread_mutex_unlock(&ack_sent_mutex);

        if(continue_flag){
            
            continue;
        }

        if (packet_itr_done) {
            //printf("Itr done received\n");
            // Done packet received. Set itr_done flag.
            pthread_mutex_lock(&itr_done_mutex);
            itr_done = 1;
            pthread_mutex_unlock(&itr_done_mutex);
        }
        else {
            // Data packet received. Store data in buffer
    //        printf("Here 3 %d\n", seq_num);
            pthread_mutex_lock(&mainBuf_mutex);
            memcpy(&mainBuf[seq_num*UDP_DATA_SIZE], &localBuf[5], numbytes-5);
            pthread_mutex_unlock(&mainBuf_mutex);

            // Update hash
      //      printf("Here 4\n");
            pthread_mutex_lock(&hash_mutex);
            remaining_packet_hash.erase(seq_num);
            hash_size = remaining_packet_hash.size();
            pthread_mutex_unlock(&hash_mutex);
        }
        
        //printf("Here 5\n");
    }

    
}

// Driver code
int main(int argc, char *argv[])
{

    //struct addrinfo*5];
    // Set up receiving sockets
    printf("File to write %s\n", argv[1]);
    recv_sockfd[0] = SetupUDPSocket(RECV_T1_PORT, SERVER_IP);
    recv_sockfd[1] = SetupUDPSocket(RECV_T2_PORT, SERVER_IP);
    recv_sockfd[2] = SetupUDPSocket(RECV_T3_PORT, SERVER_IP);
    recv_sockfd[3] = SetupUDPSocket(RECV_T4_PORT, SERVER_IP);
    recv_sockfd[4] = SetupUDPSocket(RECV_T5_PORT, SERVER_IP);

    // Set up sending socket   
    send_sockfd = SetupUDPSocket(SEND_PORT, SERVER_IP);
    
    struct addrinfo hints;
    struct addrinfo *servinfo;
    int status;

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints); // make sure hints is empty
    hints.ai_family = AF_INET;       // use IPv4
    hints.ai_socktype = SOCK_DGRAM;  // use datagram sockets

    // error checking for getaddrinfo
    // getaddrinfo used to get server address

    if ((status = getaddrinfo(CLIENT_IP, SEND_PORT, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
 
    for (int i = 0; i <= MAX_SEQ_NUM; i++ ) {
        remaining_packet_hash.insert(i);
    }

    mainBuf = new char[MAIN_BUF_SIZE];
    pthread_t tid[11];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&tid[i], NULL, recv_thread, (void*) &recv_sockfd[i]);
    }
    pthread_create(&tid[NUM_THREADS], NULL, send_thread, (void*) servinfo);
    
    for (int i = 0; i < NUM_THREADS+1; i++) {
        pthread_join(tid[i], NULL);
    }

    for (int i = 0; i < 5; i++) {
        close(recv_sockfd[i]);
    }
    close(send_sockfd);
    printf("Done Receiving\n");
    printf("Writing File\n");
    auto myfile = std::fstream(argv[1], std::ios::out | std::ios::binary);
    myfile.write((char*)&mainBuf[0], MAIN_BUF_SIZE);
    myfile.close();
    printf("Transfer complete!\n");
    return 0;
}
