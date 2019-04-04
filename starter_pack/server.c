#include "ci_tcp.h"

int main( int argc, char *argv[] )
{
    struct sockaddr_storage their_addr;
    ci_conn_t con;
    int addr_len;

    if( ci_socket( LISTEN, &con, "127.0.0.1") < 0 )
    {
        printf("error\n");
    }

    char buf[ 1440 ];
    int buf_len = 1440;

    do
    {
        ci_recv( &con, buf, buf_len, 1 );
        printf("%s\n", buf );
    }while( strcmp( buf, "quit") != 0 );
    
    ci_close( &con );

    return 0;
}