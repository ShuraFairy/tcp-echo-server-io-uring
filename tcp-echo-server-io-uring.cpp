#include <iostream>
#include <string>
#include <liburing.h>

#include "echo-server.h"

int main(int argc, char* argv[])
{
    int portno = strtol(argv[1], NULL, 10);

    EchoServer echo_server(portno);
    echo_server.initEchoServer();
    echo_server.start();

    return EXIT_SUCCESS;;
}
