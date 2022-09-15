#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/time.h>
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <string>
#include <iostream>
#include <fstream>
#include <vector>    
#include <random>
#include <iterator>
#include <algorithm>
#include <pthread.h>

#define PORT     8080 
#define MAXLINE 1024
#define BUFFER_SIZE 1024*1024 
#define MAIN_BUF_SIZE 65536*256
#define UDP_SIZE 65507 //Max UDP payload size
#define UDP_DATA_SIZE UDP_SIZE - 4
#define ACK_SIZE 4096
using namespace std;

pthread_mutex_t queue_mutex;

template<typename Iter, typename RandomGenerator>
Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
    std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return select_randomly(start, end, gen);
}

vector<int> seq_num_queue;
char* mainBuf = new char[MAIN_BUF_SIZE];
int sock_fd;
struct sockaddr_in servaddr; 

void access_queue(vector<int> & queue, int &queue_size, int &sel_seq_no) {
    pthread_mutex_lock(&queue_mutex);
    queue_size = queue.size();
    sel_seq_no = *select_randomly(queue.begin(), queue.end());
    pthread_mutex_unlock(&queue_mutex);
}

void load_smallBuf (int seq_no, int queue_size, char *smallBuf) {

        memcpy(&smallBuf[4], &mainBuf[seq_no*UDP_DATA_SIZE], UDP_DATA_SIZE);
        // Put sequence number into first two bytes
        smallBuf[0] = (seq_no >> 8) & 0xFF;
        smallBuf[1] = seq_no & 0xFF;

	// Put the queue size into second two bytes
	smallBuf[2] = (queue_size >> 8) & 0xFF;
        smallBuf[3] = queue_size & 0xFF;
}

void *send_thread (void*) {
    
    printf("Send Thread started\n");
    int queue_size, sel_seq_no;
    access_queue(seq_num_queue, queue_size, sel_seq_no);

    char smallBuf[UDP_SIZE];

    while(queue_size) {
        //Send chunk from file
  	load_smallBuf(sel_seq_no, queue_size, smallBuf);
        printf("Sending Seq no: %d, queue size %d\n", sel_seq_no, queue_size);
        sendto(sock_fd, smallBuf, UDP_SIZE, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	access_queue(seq_num_queue, queue_size, sel_seq_no);
        usleep(50000);
    }
}

void parse_ack_packet (char * recvBuffer, int recvBufSize, vector<int> & recvd_acks) {
    
    string incoming_num = ""; 
    for (int i = 0; i < recvBufSize; i++) {

        //Exit if end of line reached
	if (recvBuffer[i] == '\0') {
			
            break;
        } 

	//Command fields are separated by a space
        if (recvBuffer[i] == ' ') {

            // Remove seq num from queue if acked
            int seq_to_remove = stoi(incoming_num);
	    recvd_acks.push_back(seq_to_remove);
            incoming_num = "";
	    continue;
	}

	incoming_num += recvBuffer[i];
    }
}

void remove_seq_no(vector<int> & seq_num_queue, vector<int> &recvd_acks, int &queue_size) {
    pthread_mutex_lock(&queue_mutex);
    for (auto &recvd_ack : recvd_acks) {
        printf("recvd ack %d\n", recvd_ack);
        std::vector<int>::iterator position = std::find(seq_num_queue.begin(), seq_num_queue.end(), recvd_ack);
        if (position != seq_num_queue.end()) // == myVector.end() means the element was not found
            seq_num_queue.erase(position);
    }
    queue_size = seq_num_queue.size();
    pthread_mutex_unlock(&queue_mutex);
}

void *recv_thread (void*) {

    printf("Recv Thread Started\n");
    while (1) {

	char recvBuffer[ACK_SIZE];
        int servAddrLen = sizeof(servaddr); 
        int n = recvfrom(sock_fd, (char *)recvBuffer, ACK_SIZE,  
            MSG_WAITALL, (struct sockaddr *) &servaddr, (socklen_t*) &servAddrLen); 

	recvBuffer[n] = '\0';
        printf("Received %s\n", recvBuffer);
        vector<int> recvd_acks;
	parse_ack_packet(recvBuffer, n, recvd_acks);
        int queue_size;
        remove_seq_no(seq_num_queue, recvd_acks, queue_size);

        if (queue_size == 0)
            break;
    }
}



// Driver code 
int main() { 
    
    ifstream bigFile("short.data");

    int count = 0; 
    char buffer[1024];
    bigFile.read(mainBuf, MAIN_BUF_SIZE);
    
    if ( (sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
    
    // memset(&servaddr, 0, sizeof(servaddr)); 
        
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_addr.s_addr = inet_addr("10.0.2.240"); 
        
    // Initialize queue vector
    int max_seq_num = 256;
    for (int i = 0; i < max_seq_num; i++) {
        seq_num_queue.push_back(i);
    }
    
    printf("Starting threads\n"); 
    pthread_t tid[2];
    
    pthread_create(&tid[0], NULL, send_thread, NULL);
    pthread_create(&tid[1], NULL, recv_thread, NULL);

    pthread_join(tid[0],NULL);
    pthread_join(tid[1],NULL);
    close(sock_fd); 
    return 0; 
}
