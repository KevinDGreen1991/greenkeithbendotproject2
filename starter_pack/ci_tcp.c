#include "ci_tcp.h"
#include "ci_packet.h"

// use this for debugging purpsoes.
void print_packet(ci_packet_t *pkt) {
    printf("ident: 0x%x" PRIu16 "\n",pkt->hdr.identifier);
    printf("src port: %" PRIu16 "\n", pkt->hdr.src_port);
    printf("dst port: %" PRIu16 "\n", pkt->hdr.dst_port);
    printf("seq: %" PRIu32 "\n", pkt->hdr.seq);
    printf("ack: %" PRIu32 "\n", pkt->hdr.ack);
    printf("hlen: %" PRIu16 "\n",pkt->hdr.hlen);
    printf("plen: %" PRIu16 "\n",pkt->hdr.plen);
    printf("flag: %u\n", pkt->hdr.flags);
    printf("window: %" PRIu16 "\n",pkt->hdr.window);
    printf("data: %s\n", pkt->data);
}

ci_packet_t *make_packet(ci_conn_t *con, char *data, int len, uint32_t seq, uint32_t ack, 
                            uint16_t window, uint8_t flags) {
    ci_packet_t *pkt = malloc(sizeof(ci_packet_t));

    pkt->hdr.identifier = (uint16_t) TEAM_IDENTIFIER;
    pkt->hdr.src_port = con->src_port;
    pkt->hdr.dst_port = con->dst_port;
    pkt->hdr.seq = seq;
    pkt->hdr.ack = ack;
    pkt->hdr.hlen = HEADER_LEN;
    pkt->hdr.plen = HEADER_LEN + len;
    pkt->hdr.flags = flags;
    pkt->hdr.window = window;

    if(len > 0) {
        strcpy(pkt->data, data);
    }

    return pkt;
}

void send_data(ci_conn_t *con, char *data, int len) {
    ci_packet_t *pkt;
    while(len > 0) {
        pkt = make_packet(con, data, len, con->last_ack, con->last_ack, 0, 0);
        // send the packet itself.
        sendto(con->sfd, pkt, pkt->hdr.plen + C_PADDING, 0, con->serv_info.ai_addr, con->serv_info.ai_addrlen);
        recv_data(con, 0, 1); // hoping to get an ack, timeout otherwise

        len -= len;
    }
}

void recv_data(ci_conn_t *con, int wait, int timeout) {
    struct sockaddr_storage their_addr;
    int addr_len;
    int buff_len = MAX_DATA + HEADER_LEN + C_PADDING;
    char buffer[buff_len];
    // time out handler using select()
    // See UNIX Networking Book for details Chapter 14 section 2
    fd_set afd; // timeout if no acknowledgement
    struct timeval timeout_time;
    timeout_time.tv_sec = TIMEOUT_SEC_DEFAULT;
    timeout_time.tv_usec = TIMEOUT_MICROSEC_DEFAULT;

    pthread_mutex_lock(&(con->recv_lock));

    // just take a peeksy of the recvfrom buffer
    if(wait) {
        buff_len = recvfrom(con->sfd, buffer, buff_len, MSG_PEEK, (struct sockaddr *)&their_addr, &addr_len);
    } else if(timeout) {
        // wait for something read, give up after a poorly selected default time
        FD_ZERO(&afd);
        FD_SET(con->sfd, &afd);
        select(con->sfd + 1, &afd, NULL, NULL, &timeout_time);
    } else {
        buff_len = recvfrom(con->sfd, buffer, buff_len, MSG_DONTWAIT | MSG_PEEK, (struct sockaddr *)&their_addr, &addr_len);
    }
    // oh my, there is data, let's get it.
    if(buff_len > 0) {
        ci_packet_t *pkt = malloc(sizeof(ci_packet_t));
        int read = 0;
        // get it all out from the boofer
        while(read < buff_len) {
            read = recvfrom(con->sfd, pkt + read, buff_len - read, 0, (struct sockaddr *)&their_addr, &addr_len);
            read += buff_len;
        }
        con->recv_buffer_len = read;
        con->recv_buffer = malloc(read - HEADER_LEN - C_PADDING);
        strcpy(con->recv_buffer, pkt->data);

        // helps with debugging.
        // print_packet(pkt);

        // handle the packet based on what we got
        // if it's an ACK, see if we should acknowledge it
        // anything else, send an acknoledgement!
        if(pkt->hdr.flags == ACK) {
            // update acknowledgement
            pthread_mutex_lock(&(con->ack_lock));
            if(pkt->hdr.ack > con->last_ack) {
                con->last_ack = pkt->hdr.ack;
            }
            pthread_mutex_unlock(&(con->ack_lock));
        } else {
            // send an acknowledgement
            ci_packet_t *reply;
            reply = make_packet(con, "", HEADER_LEN, pkt->hdr.seq, pkt->hdr.seq + 1, 0, ACK);
            sendto(con->sfd, reply, reply->hdr.plen + C_PADDING, 0, (struct sockaddr *)&their_addr, addr_len);

            con->last_seq = pkt->hdr.seq;
        }
    }

    pthread_mutex_unlock(&(con->recv_lock));
}

void *main_loop(void *con_info) {
    ci_conn_t *con = (ci_conn_t *) con_info;
    int closing;

    char *send_buffer;
    int send_len;

    for(;;) { 
        // check if we're quiting...
        pthread_mutex_lock(&(con->closing_lock));
        closing = con->closing;
        pthread_mutex_unlock(&(con->closing_lock));

        // get ready to send data, unless nothing else to do and quitting.
        pthread_mutex_lock(&(con->send_lock));
        // if we're closing the connection and there is nothing left to send, get out of here
        if(closing && con->send_buffer_len == 0) {
            break;
        }

        if(con->send_buffer_len > 0) {
            // copy the data from the connection to the send buffer
            send_buffer = malloc(con->send_buffer_len);
            strcpy(send_buffer, con->send_buffer);
            send_len = con->send_buffer_len;
            // clear our send buffer
            free(con->send_buffer);
            con->send_buffer = NULL;
            con->send_buffer_len = 0;

            pthread_mutex_unlock(&(con->send_lock));
            send_data(con, send_buffer, send_len);
            free(send_buffer);
        } else {
            pthread_mutex_unlock(&(con->send_lock));
        }
        recv_data(con, 0, 0);

        int stop_waiting = 0;
        pthread_mutex_lock(&(con->recv_lock));
        if(con->recv_buffer_len > 0) {
            stop_waiting = 1;
        } 
        pthread_mutex_unlock(&(con->recv_lock));

        if(stop_waiting) {
            pthread_cond_signal(&(con->recv_wait));
        }
    }

    // close out.
    // ci_close should already have handled closing the socket and freeing info
    // will join the exited thread.
    pthread_exit(NULL);
    return NULL;
}