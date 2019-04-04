#ifndef CI_TCP_H_
#define CI_TCP_H_
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h> 
#include <pthread.h>
#include <inttypes.h>
#include "ci_packet.h"

// based on a project from CMU Networking Course
// with modifications

// it's over 9000.
#define PORT "9001" 

// so the network does not split up our packets, avoid going over the typical 1440 bytes
// we use 1337 because we're elite
#define MAX_DATA 1337

#define TIMEOUT_SEC_DEFAULT 1
#define TIMEOUT_MICROSEC_DEFAULT 0

typedef struct
{
    int sfd;                            // socket file descriptor
    struct sockaddr_in conn;
    struct addrinfo serv_info;

    uint16_t dst_port;                  // destination port
    uint16_t src_port;                  // destination port

    int send_buffer_len;                // length of data to send
    char *send_buffer;                  // data to send
    pthread_mutex_t send_lock;          

    int recv_buffer_len;
    char *recv_buffer;
    pthread_mutex_t recv_lock;
    pthread_cond_t recv_wait;

    pthread_t tid;                      // thread id

    int closing;                        // set to 1 when we begin closing the connection
    pthread_mutex_t closing_lock;       

    pthread_mutex_t ack_lock;
    uint32_t last_ack;                       // window information
    uint32_t last_seq;                       // last ack number received and last sequence number sent
    
    int serverClient;
} ci_conn_t;

#define LISTEN 0
#define CONNECT 1

// API calls for developers using our version of TCP
int ci_socket( int type, ci_conn_t *con, char *ip );
int ci_send( ci_conn_t *con, char *buffer, int len );
int ci_recv( ci_conn_t *con, char *dest, int len, int wait );
int ci_close( ci_conn_t *con );

// Back end "hidden" information the developers do not see
void print_packet( ci_packet_t *pkt );
void *main_loop( void *con_info );
void recv_data( ci_conn_t *con, int wait, int timeout );
void send_data( ci_conn_t *con, char *data, int len );


#endif