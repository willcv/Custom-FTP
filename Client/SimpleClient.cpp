#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <string>
#include <iostream>
#include <fstream>
    
#define PORT     8080 
#define MAXLINE 1024
#define BUFFER_SIZE 1024*1024 
#define MAIN_BUF_SIZE 65536*256
#define UDP_SIZE 65507
#define UDP_DATA_SIZE UDP_SIZE - 4
using namespace std;

// Driver code 
int main() { 
    int sockfd; 
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
        
    int n, len; 
    int sequence_number = 0;
    int max_seq_num = 256;
    while(sequence_number < max_seq_num){

	
    	memcpy(&smallBuf[4], &mainBuf[sequence_number*UDP_DATA_SIZE], UDP_DATA_SIZE);
        smallBuf[0] = (sequence_number >> 8) & 0xFF;
        smallBuf[1] = sequence_number & 0xFF;

	printf("%d\n", (unsigned char) smallBuf[10]);
        for (int i = 0; i < 10; i++)
        {
            printf("%02X", (unsigned char) smallBuf[i]);
        }
	printf("\n");
        sendto(sockfd, smallBuf, UDP_SIZE, 
            MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr)); 
        
	printf("Sequence No Sent: %d\n", sequence_number);
        sequence_number++;
    }
            
    //n = recvfrom(sockfd, (char *)buffer, MAXLINE,  
    //            MSG_WAITALL, (struct sockaddr *) &servaddr, (socklen_t*) &len); 
    //buffer[n] = '\0'; 
    //printf("Server : %s\n", buffer); 
    
    close(sockfd); 
    return 0; 
}
