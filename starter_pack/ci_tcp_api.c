#include "ci_tcp.h"

int ci_socket(int type, ci_conn_t *con, char *ip) {
    int sfd;
    struct addrinfo *servinfo, *p, hints;
    struct sockaddr_in myaddr;

    // initial data and initialize mutexes
    con->closing = 0;
    con->send_buffer_len = 0;
    pthread_mutex_init(&(con->closing_lock), NULL);
    con->send_buffer_len = 0;
    con->send_buffer = NULL;
    pthread_mutex_init(&(con->send_lock), NULL);
    con->recv_buffer_len = 0;
    con->recv_buffer = NULL;
    pthread_mutex_init(&(con->recv_lock), NULL);
    pthread_cond_init(&(con->recv_wait), NULL);
    con->dst_port = atoi(PORT);
    pthread_mutex_init(&(con->ack_lock), NULL);
    con->last_ack = 0;
    con->last_seq = 0;
    
    switch(type) {
        case LISTEN:
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET; 
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_flags = AI_PASSIVE;

            if (getaddrinfo(NULL, PORT, &hints, &servinfo) < 0) {
                fprintf(stderr, "getaddrinfo: %s\n", strerror(errno));
                return -1;
            }

            for(p = servinfo; p != NULL; p = p->ai_next) {
                if ((sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                    fprintf(stderr, "socket(): %s\n", strerror(errno));
                    continue;
                }
                con->sfd = sfd;

                if (bind(sfd, p->ai_addr, p->ai_addrlen) == -1) {
                    close(sfd);
                    fprintf(stderr, "bind(): %s\n", strerror(errno));
                    continue;
                }
                break;
            }
            break;
        case CONNECT:
            if(ip == NULL) {
                fprintf(stderr, "ci_socket() error: No IP address specified.\n");
                return -1;
            }

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;

            if (getaddrinfo(ip, PORT, &hints, &servinfo) < 0) {
                fprintf(stderr, "getaddrinfo(): %s\n", strerror(errno));
                return -1;
            }

            for(p = servinfo; p != NULL; p = p->ai_next) {
                if ((sfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                    fprintf(stderr, "socket(): %s\n", strerror(errno));
                    continue;
                }

                con->sfd = sfd;
                break;
            }
            con->serv_info = (struct addrinfo) *p;
        
            break;  
        default:
            fprintf(stderr, "Unknown type provided to ci_socket().\n");
            return -1;
    }
    int len = sizeof(struct sockaddr_in *);
    getsockname(sfd, (struct sockaddr *) &myaddr, &len);
    con->src_port = ntohs(myaddr.sin_port);

    // start the main thread loop
    pthread_create(&(con->tid), NULL, main_loop, (void *)con);

    return 0;
}

int ci_close(ci_conn_t *con) {
    pthread_mutex_lock(&(con->closing_lock));
    con->closing = 1;
    pthread_mutex_unlock(&(con->closing_lock));

    pthread_join(con->tid, NULL);

    if(con->recv_buffer_len > 0) {
        free(con->recv_buffer);
    }
    if(con->send_buffer_len > 0) {
        free(con->send_buffer);
    }

    return close(con->sfd);
}

int ci_send(ci_conn_t *con, char *buffer, int len) {
    pthread_mutex_lock(&(con->send_lock));

    if(con->send_buffer != NULL) {
        // already stuff here!
        // allocate more memory and increase length
        con->send_buffer = realloc(con->send_buffer, con->send_buffer_len + len);
        // copy the new buffer into the send buffer but after the old buffer.
        strcpy(con->send_buffer + con->send_buffer_len + 1, buffer);
    } else {
        // new stuff! easy.
        con->send_buffer = malloc(len);
        strcpy(con->send_buffer, buffer);
    }
    // don't forget to save the length!
    con->send_buffer_len += len;



    pthread_mutex_unlock(&(con->send_lock));
    return 0;
}

int ci_recv(ci_conn_t *con, char *dest, int len, int wait) {
    pthread_mutex_lock(&(con->recv_lock));

    if(wait) pthread_cond_wait(&(con->recv_wait), &(con->recv_lock));

    int recv_len = 0;
    if(con->recv_buffer_len > 0) {
        recv_len = con->recv_buffer_len;
        if(con->recv_buffer_len > len) {
            // more than we can chew
            // want to avoid out of bounds on dest
            recv_len = len;
        }
        // API callers fault if this fails
        strncpy(dest, con->recv_buffer, recv_len);

        if(recv_len < con->recv_buffer_len) {
            int rest_of = con->recv_buffer_len - recv_len;
            char *left_over = malloc(rest_of);
            strncpy(left_over, con->recv_buffer + recv_len, rest_of);
            free(con->recv_buffer);
            con->recv_buffer = left_over;
            con->recv_buffer_len = rest_of;
        } else {
            free(con->recv_buffer);
            con->recv_buffer = NULL;
            con->recv_buffer_len = 0;
        }
    }

    pthread_mutex_unlock(&(con->recv_lock));

    return recv_len;
}
