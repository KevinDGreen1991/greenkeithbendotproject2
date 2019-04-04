#include "ci_tcp.h"
#include <string.h>

int main(int argc, char *argv[])
{
    ci_conn_t con;

    if( ci_socket( CONNECT, &con, "127.0.0.1") < 0 )
    {
        printf("error\n");
    }

    char buff[ 1024 ];
    do
    {
        memset( buff, '\0', 1024 );
        printf("> ");
        scanf("%s", buff );
        getchar();
        ci_send( &con, buff, strlen( buff ) );
    }while( strcmp( buff, "quit") != 0 );
    
    ci_close( &con );

    return 0;
}