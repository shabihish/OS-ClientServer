#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <limits.h>

#define MAX_ROOM_CONNECTIONS 3
#define LOOPBACK_ADDR "127.0.0.1"
#define BASE_ASSIGNED_BROADCAST_PORT 9000


void print_err_and_quit(char *msg) {
    char *str = "\033[0;31m";
    write(1, str, strlen(str));
    write(1, msg, strlen(msg));
    write(1, "\n", 1);
    str = "\033[0;0m";
    write(1, str, strlen(str));
    exit(EXIT_SUCCESS);
}

void print_successs_msg(char *msg) {
    char *str = "\033[0;32m";
    write(1, str, strlen(str));
    write(1, msg, strlen(msg));
    write(1, "\n", 1);
    str = "\033[0;0m";
    write(1, str, strlen(str));
}

int connect_server(int port) {
    struct sockaddr_in server_address;
    int fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr(LOOPBACK_ADDR);

    if (connect(fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
        print_err_and_quit("Could not connect to server. Quitting...");

    return fd;
}

int main(int argc, char *argv[]) {
    int port = atoi(argv[1]);
    if (port <= 0)
        print_err_and_quit("Invalid port number given. Quitting ");

    char buffer[2048] = {0};
    char print_buffer[1024];
    int fd;
    int num_of_bytes = 0;

    fd = connect_server(port);
    print_successs_msg("Connection to server has been established.");
    num_of_bytes = recv(fd, buffer, sizeof(buffer), 0);

    write(1, buffer, num_of_bytes);

    read(0, buffer, sizeof(buffer));
    send(fd, buffer, strlen(buffer), 0);

    num_of_bytes = recv(fd, buffer, sizeof(buffer), 0);
    write(1, buffer, num_of_bytes);



    return 0;
}