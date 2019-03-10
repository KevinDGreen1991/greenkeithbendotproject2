#ifndef CI_PACKET_H_
#define CI_PACKET_H_
#include <inttypes.h>

// Use your assigned team identifier here
#define TEAM_IDENTIFIER 0x1337

// Change these only if you add to the header
// C_padding will also change
#define HEADER_LEN 21
#define C_PADDING 3
// C wants 24 bytes even, so it pads the header 
// struct with 3 additional bytes...just to mess 
// with me...

#define SYN     0x01
#define ACK     0x02
#define SYNACK  0x03
#define FIN     0x04
#define RST     0x08

typedef struct {
    uint16_t identifier;    // Team Identifier
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint16_t hlen;          // Header length
    uint16_t plen;          // Packet length
    uint8_t  flags;         // bit flags
    uint16_t window;        // advertised window size
} ci_header_t;

// encapsulation anyone?
typedef struct {
    ci_header_t hdr;
    char data[1337 + HEADER_LEN];
} ci_packet_t;

#endif