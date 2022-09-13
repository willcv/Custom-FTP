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


#define PORT     8080 
#define MAXLINE 1024
#define BUFFER_SIZE 1024*1024 
#define MAIN_BUF_SIZE 65536*256
#define UDP_SIZE 65507 //Max UDP payload size
#define UDP_DATA_SIZE UDP_SIZE - 4
#define ACK_SIZE 4096
using namespace std;

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

// Driver code 
int main() { 
    int sockfd; 
    fd_set readfds, masterfds;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    
    string hello = "Hello from client"; 
    struct sockaddr_in     servaddr; 
    
    ifstream bigFile("short.data");
    char* mainBuf = new char[MAIN_BUF_SIZE];

    int count = 0; 
    char buffer[1024];
    bigFile.read(mainBuf, MAIN_BUF_SIZE);
    
   // while (bigFile) {
   // 	bigFile.read(buffer, bufferSize);
   //     // process data in buffer
   //     memcpy(&smallBuf, &buffer, 128);
   //     printf("%d: %s\n", count, smallBuf);
   //	count++;
   // }

    char smallBuf[UDP_SIZE];




    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
    
    memset(&servaddr, 0, sizeof(servaddr)); 
        
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(PORT); 
    servaddr.sin_addr.s_addr = inet_addr("10.0.2.102"); 
        
    // Initialize queue vector
    int max_seq_num = 256;
    vector<int> seq_num_queue;
    for (int i = 0; i < max_seq_num; i++) {
        seq_num_queue.push_back(i);
    }

   FD_ZERO(&masterfds);
   FD_SET(sockfd, &masterfds);


    int n, len; 
    int sequence_number = 0;
    int loop_count = 1;
    while(seq_num_queue.size()){

	int curr_seq_num = *select_randomly(seq_num_queue.begin(), seq_num_queue.end());
    	
        memcpy(&smallBuf[4], &mainBuf[curr_seq_num*UDP_DATA_SIZE], UDP_DATA_SIZE);
        // Put sequence number into first two bytes
        smallBuf[0] = (curr_seq_num >> 8) & 0xFF;
        smallBuf[1] = curr_seq_num & 0xFF;

	// Put the queue size into second two bytes
	int queue_size = seq_num_queue.size();
	printf("seq_queue size %d\n", (int) queue_size);
	smallBuf[2] = (queue_size >> 8) & 0xFF;
        smallBuf[3] = queue_size & 0xFF;

	printf("%d\n", (unsigned char) smallBuf[10]);
        for (int i = 0; i < 10; i++)
        {
            printf("%02X", (unsigned char) smallBuf[i]);
        }
	printf("\n");
        sendto(sockfd, smallBuf, UDP_SIZE, 
            MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr)); 
        
	printf("Sequence No Sent: %d\n", curr_seq_num);

	char recvBuffer[ACK_SIZE];
        int servAddrLen = sizeof(servaddr); 
        n = recvfrom(sockfd, (char *)recvBuffer, ACK_SIZE,  
            MSG_WAITALL, (struct sockaddr *) &servaddr, (socklen_t*) &servAddrLen); 
            
        recvBuffer[n] = '\0'; 
        printf("Server : %s\n", recvBuffer); 

	// Parse numbers from ack packet
        string incoming_num = "";	 
        for (int i = 0; i < n; i++) {

            //Exit if end of line reached
	    if (recvBuffer[i] == '\0') {
			
                break;
            } 

	    //Command fields are separated by a space
            if (recvBuffer[i] == ' ') {

                // Remove seq num from queue if acked
		int seq_to_remove = stoi(incoming_num);
		printf("Seq to remove: %d\n", seq_to_remove);
                std::vector<int>::iterator position = std::find(seq_num_queue.begin(), seq_num_queue.end(), seq_to_remove);
                if (position != seq_num_queue.end()) // == myVector.end() means the element was not found
                    seq_num_queue.erase(position);
		printf("Seq queue size: %d\n", (int) seq_num_queue.size());
		incoming_num = "";
		continue;
	    }

	    incoming_num += recvBuffer[i];
        }

	loop_count ++;

    }
            
    //n = recvfrom(sockfd, (char *)buffer, MAXLINE,  
    //            MSG_WAITALL, (struct sockaddr *) &servaddr, (socklen_t*) &len); 
    //buffer[n] = '\0'; 
    //printf("Server : %s\n", buffer); 
    
    close(sockfd); 
    return 0; 
}
