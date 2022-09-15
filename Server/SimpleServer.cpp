#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <unistd.h>
#include <pthread.h>

#define PORT     8080 
#define UDP_SIZE 65507
#define UDP_DATA_SIZE UDP_SIZE - 4
#define ACK_SIZE 4096
#define MAIN_BUF_SIZE 65536*256
#define MAX_QUEUE_SIZE 256

using namespace std;

pthread_mutex_t ack_string_mutex;
pthread_mutex_t done_mutex;

int sockfd;
struct sockaddr_in servaddr, cliaddr; 
int done = 0;
char* mainBuf = new char[MAIN_BUF_SIZE];
string ack_packet_string;

void remove_seq_no (vector<int> &seq_num_queue, int seq_to_remove) {
    std::vector<int>::iterator position = std::find(seq_num_queue.begin(), seq_num_queue.end(), seq_to_remove);
    if (position != seq_num_queue.end()) // == myVector.end() means the element was not found
        seq_num_queue.erase(position);
}

void *send_thread (void*) {

    int queue_size = MAX_QUEUE_SIZE;
    int prev_queue_size = MAX_QUEUE_SIZE;
    int loop_counter = 1;
    
    int len = sizeof(cliaddr);  //len is value/result 
    
    
    vector<int> remaining_seq_num;

    for (int i = 0; i < MAX_QUEUE_SIZE; i++) {
        remaining_seq_num.push_back(i);
    }
    vector<int> ack_packet_list;
    while (queue_size) {

        printf("newloop \n");
        unsigned char UDP_recvBuf[UDP_SIZE]; 
        int n = recvfrom(sockfd, (char *) UDP_recvBuf, UDP_SIZE,  
                MSG_WAITALL, ( struct sockaddr *) &cliaddr, (socklen_t*)&len); 

        int seq_num = (UDP_recvBuf[0] << 8) | UDP_recvBuf[1] & 0xFF;
        int queue_size = (UDP_recvBuf[2] << 8) | UDP_recvBuf[3] & 0xFF;
        printf("Sequence Number: %d\n", seq_num);
        printf("Queue Size: %d, %d, %d\n", queue_size, UDP_recvBuf[2], UDP_recvBuf[3]);
        memcpy(&mainBuf[seq_num*UDP_DATA_SIZE], &UDP_recvBuf[4], UDP_DATA_SIZE);

        remove_seq_no(remaining_seq_num, seq_num);
        ack_packet_list.push_back(seq_num);
        
        if (loop_counter % 5 == 0) {
            for (int i = 0; i < prev_queue_size - queue_size; i++) {
                ack_packet_list.erase(ack_packet_list.begin());
            }
            printf("waiting for mutex\n");
            pthread_mutex_lock(&ack_string_mutex);
    	    ack_packet_string = "";
            
            for (auto & ack : ack_packet_list) {
                ack_packet_string += to_string(ack) + " ";
            }
            pthread_mutex_unlock(&ack_string_mutex);
            prev_queue_size = queue_size;
        }
        loop_counter ++;
    }
    printf("Send thread done\n");
    pthread_mutex_lock(&done_mutex);
    done = 1;
    pthread_mutex_unlock(&done_mutex);
}

void *recv_thread (void*) {
    printf("Starting recv thread\n");
    while (1) {
        
        usleep(50000);
        string temp_ack_string;
        pthread_mutex_lock(&ack_string_mutex);
        temp_ack_string = ack_packet_string;
        pthread_mutex_unlock(&ack_string_mutex);


        if (temp_ack_string.length() > 0) { 
            
	    int temp_done = 0;
	    pthread_mutex_lock(&done_mutex);
            temp_done = done;
            pthread_mutex_unlock(&done_mutex);
	    
            if (temp_done) {
                for(int i = 0; i < 5; i++) {

                    sendto(sockfd, temp_ack_string.c_str(), temp_ack_string.length(), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, sizeof(cliaddr));
                    printf("Sent %s\n", temp_ack_string.c_str()); 
		
                }
                break;
            }
            sendto(sockfd, temp_ack_string.c_str(), temp_ack_string.length(), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, sizeof(cliaddr));
            printf("Sent %s\n", temp_ack_string.c_str()); 
        } else {
            usleep(200);
        }

    }
}

    
// Driver code 
int main() { 

    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
        
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
        
    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(PORT); 
        
    // Bind the socket with the server address 
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,  
            sizeof(servaddr)) < 0 ) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
        
    printf("Starting threads\n");
    pthread_t tid[2];

    pthread_create(&tid[0], NULL, send_thread, NULL);
    pthread_create(&tid[1], NULL, recv_thread, NULL);    

    pthread_join(tid[0],NULL);
    pthread_join(tid[1],NULL);
    close(sockfd);

    auto myfile = std::fstream("file.out", std::ios::out | std::ios::binary);
    myfile.write((char*)&mainBuf[0], MAIN_BUF_SIZE);
    myfile.close();
    return 0; 
}
