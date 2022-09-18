#ifndef COMMON
#define COMMON

#define NUM_THREADS 4
#define FILE_SIZE 65536*16384 //16*1024*1024//65536*16384
#define UDP_SIZE 1472
#define UDP_DATA_SIZE (UDP_SIZE - 5)

#define SERVER_IP                                "10.0.2.66"
#define SERVER_MAIN_PORT                         "25581"
static const char* const SERVER_THREAD_PORTS[] = {"25581", "25582", "25583", "25584", "25585"};


#define CLIENT_IP                                "10.0.1.115"
#define CLIENT_MAIN_PORT                         "25580"
static const char* const CLIENT_THREAD_PORTS[] = {"26581", "26582", "26583", "26584", "26585"};

#endif
