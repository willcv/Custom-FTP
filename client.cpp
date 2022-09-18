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
#include <chrono>
#include <unordered_set>
#include "safequeue.h"
#include "common.h"

using namespace std;
using namespace std::chrono;


ThreadsafeQueue<int> send_queue;
char *main_buf;

pthread_mutex_t ack_done_mutex;
pthread_mutex_t thread_done_mutex;
pthread_mutex_t final_ack_recvd_mutex;
pthread_mutex_t measure_time_mutex;

int send_thread_done = 0;
int ack_done = 1;
int final_ack_recvd = 0;
int start_set = 0;
std::chrono::high_resolution_clock::time_point stop;
std::chrono::high_resolution_clock::time_point start;

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
    int file_last_index = (FILE_SIZE - 1) / UDP_DATA_SIZE;
    printf("Starting Thread\n");

    int local_final_ack_recvd = 0;

    pthread_mutex_lock(&measure_time_mutex);
    if (!start_set) {
        start = std::chrono::high_resolution_clock::now();
        start_set = 1;
        cout << "Starting Timestamp: " << duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() << endl;

    }
    pthread_mutex_unlock(&measure_time_mutex);
    while (!local_final_ack_recvd) {

        int local_ack_done;
        pthread_mutex_lock(&ack_done_mutex);
        local_ack_done = ack_done;
        pthread_mutex_unlock(&ack_done_mutex);
        if (!local_ack_done){
            continue;
        }
        int queue_size;
        queue_size = send_queue.size();
        while ((sequence_num = ReadQueue()) != -1)
        {
            //printf("Here 1 %d\n", sequence_num);
            memcpy(&small_buf[5], &main_buf[sequence_num * UDP_DATA_SIZE], UDP_DATA_SIZE);
            //printf("Here 2\n");
            memcpy(&small_buf, &sequence_num, sizeof(sequence_num));
            small_buf[4] = 0;
            int num_duplicate_sends;
            if (queue_size > 1000) {
                num_duplicate_sends = 1;
            } else if (queue_size > 500) {
                num_duplicate_sends = 2;
            } else if (queue_size > 250) {
                num_duplicate_sends = 3;
            }
            else {
                num_duplicate_sends = 4;
            }
            for (int i = 0; i < num_duplicate_sends; i++) {

                if ((numbytes = sendto(sock_fd, small_buf, UDP_SIZE, 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
                {
                    perror("Sending Normal Seq num packets");
                    exit(1);
                }
                // Measure the time here so we stop measuring as soon as the last bit is sent
                pthread_mutex_lock(&measure_time_mutex);
                stop = std::chrono::high_resolution_clock::now();
                pthread_mutex_unlock(&measure_time_mutex);
                // Remove delay if not needed
                usleep(400);
            }
            //printf("sent %d\n", sequence_num);
        }

        // Each bit of send_thread_done holds an indicator for the thread being done.
        // All threads are done sending when all have written a 1 to their respective bit index
        //printf("Thread %d done\n", thread_idx);
        pthread_mutex_lock(&thread_done_mutex);
        send_thread_done |= 0x01 << thread_idx;
        pthread_mutex_unlock(&thread_done_mutex);
    
        pthread_mutex_lock(&final_ack_recvd_mutex);
        local_final_ack_recvd = final_ack_recvd;
        pthread_mutex_unlock(&final_ack_recvd_mutex);

    }
    printf("Exiting Thread %d\n", thread_idx); 
    close(sock_fd);
}

void * ReceiveAckFromServer(void *arg)
{

    int send_sockfd = (intptr_t)arg;
    struct sockaddr_in cliaddr;
    socklen_t addr_len = sizeof(cliaddr);
    bool ack_received = false;
    int numbytes;
    char ack_buffer[UDP_SIZE];
    
    // Value of int when all threads have written flags
    int THREAD_DONE_VAL = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        THREAD_DONE_VAL |= (0x01 << i);
    }
    
    // Setup server address info
    struct addrinfo *servinfo;
    GetUDPServerInfo(SERVER_IP, SERVER_MAIN_PORT, servinfo);
    int local_final_ack_recvd = 0;
    int itr_num = 0;
    while (!local_final_ack_recvd) {
        
        unordered_set<int> hashset;
        int local_thread_done;
        pthread_mutex_lock(&thread_done_mutex);
        local_thread_done = send_thread_done;
        pthread_mutex_unlock(&thread_done_mutex);

        if (local_thread_done != THREAD_DONE_VAL) {
            continue;
        }

        ack_received = 0;
        pthread_mutex_lock(&ack_done_mutex);
        ack_done = 0;
        pthread_mutex_unlock(&ack_done_mutex);
 
        // Threads are done sending iteration
        // Send itr_done packets

        // Send Iteration done packet 5 times
        char done_buf[] = {'0', '0', '0', '0', '1'};
        for (int i = 0; i < 6; i++)
        {
            if ((numbytes = sendto(send_sockfd, done_buf, 5, 0, servinfo->ai_addr, servinfo->ai_addrlen)) == -1)
            {
                perror("Send Ack packets to server");
                exit(1);
            }
        }

        while (!ack_received)
        {

            if ((numbytes = recvfrom(send_sockfd, ack_buffer, UDP_SIZE, 0, (struct sockaddr *)&cliaddr, &addr_len)) == -1)
            {
                perror("Receive from Server");
                exit(1);
            }
            int read_itr_num = 0;
            memcpy(&read_itr_num, &ack_buffer[1], 3);
            if (read_itr_num != itr_num) {
                continue;
            }
            if (numbytes == 4 && ack_buffer[0] == 1)
            {
                // Set final ack global flag, then ack done global flag
                pthread_mutex_lock(&final_ack_recvd_mutex);
                final_ack_recvd = 1;
                pthread_mutex_unlock(&final_ack_recvd_mutex);
                local_final_ack_recvd = 1;
                //pthread_mutex_lock(&ack_done_mutex);
                ack_done = 1;
                //pthread_mutex_unlock(&ack_done_mutex);
                ack_received = 1;
                
                pthread_mutex_lock(&thread_done_mutex);
                send_thread_done = 0;
                pthread_mutex_unlock(&thread_done_mutex);
                break;
            }
            int temp;
            for (int i = 4; i < numbytes; i += 4)
            { 
                memcpy(&temp, &ack_buffer[i], sizeof(int));
                hashset.insert(temp);
            }
            if (ack_buffer[0])
            {
                ack_received = true;
                pthread_mutex_lock(&thread_done_mutex);
                send_thread_done = 0;
                pthread_mutex_unlock(&thread_done_mutex);
            }
        }
        for (auto it = hashset.begin(); it != hashset.end(); ++it)
        {
            send_queue.push(*it);
        }
        // Set ack done global flag
        pthread_mutex_lock(&ack_done_mutex);
        ack_done = 1;
        pthread_mutex_unlock(&ack_done_mutex);
        itr_num ++;
    }   
    printf("Recv exiting\n");
}

// Driver code
int main(int argc, char *argv[])
{

    int sock_fd = SetupUDPSocket(CLIENT_IP, CLIENT_MAIN_PORT);


    // Read file and initialize main buffer
    ifstream input_file(argv[1]);
    main_buf = new char[FILE_SIZE];

    input_file.read(main_buf, FILE_SIZE);

    // Initialize queue
    int file_last_index = (FILE_SIZE - 1) / UDP_DATA_SIZE;
    printf("file last index %d\n", file_last_index);
    for (int i = 0; i <= file_last_index; i++)
    {
        send_queue.push(i);
    }

    // Begin measuring time
    auto start = std::chrono::high_resolution_clock::now();

    bool transfer_complete = false;
    unordered_set<int> drop_sequence_num;
    int numbytes;
    pthread_t tid[7];
    std::chrono::high_resolution_clock::time_point last_sent_packet_time;
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_create(&tid[i], NULL, ClientSendTo, (void *)(intptr_t)i);
    }
    pthread_create(&tid[NUM_THREADS], NULL, ReceiveAckFromServer, (void *)(intptr_t)sock_fd);
    for (int i = 0; i < NUM_THREADS+1; i++)
    {
        pthread_join(tid[i], NULL);
    }
   
    cout << "Done with Transfer"
         << "\n";
    // Stop measuring time and calculate the elapsed time
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
    cout << "Transfer Time: " << duration.count() << "\n";
    cout << "File Transfer Complete\n";

    close(sock_fd);
    return 0;
}
