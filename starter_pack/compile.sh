#!/bin/bash

gcc ci_tcp_api.c ci_tcp.c client.c -o client -lpthread
gcc ci_tcp_api.c ci_tcp.c server.c -o server -lpthread
