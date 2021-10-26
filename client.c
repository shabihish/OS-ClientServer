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
#include <signal.h>


#define MAX_ROOM_CONNECTIONS 3
#define LOOPBACK_ADDR "127.0.0.1"
#define BROADCAST_ADDR24 "127.0.0.1"
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

void print_msg(char *msg, int msg_len, int print_end_of_line) {
    if (msg_len < 0)
        msg_len = strlen(msg);
    write(1, msg, msg_len);
    if (print_end_of_line)
        write(1, "\n", 1);
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

int get_broadcast_socket(int port) {
    struct sockaddr_in address;
    int fd;
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(BROADCAST_ADDR24);

    bind(fd, (struct sockaddr *) &address, sizeof(address));
    return fd;
}

void ask_question(int bc_fd) {
    char buffer[1024];

    print_msg("Please ask your question: \n", -1, 0);
    if (read(0, buffer, sizeof(buffer)) >= 0) {
        send(bc_fd, buffer, strlen(buffer), 0);
    }
}

void wait_for_question(int bc_fd) {
    char buffer[1024];
    int num_of_bytes = recv(bc_fd, buffer, sizeof(buffer), 0);
    print_msg("New question from client ", -1, 0);
    print_msg(buffer, num_of_bytes, 1);
}

void answer_question(int bc_fd) {
    char buffer[1024];
    print_msg("Please write your answer: ", -1, 1);
    if (read(0, buffer, sizeof(buffer)) >= 0) {
        send(bc_fd, buffer, strlen(buffer), 0);
    }
}

void wait_for_response(int bc_fd) {
    char buffer[1024];
    print_msg("Awaiting response from the other client...", -1, 1);
    int num_of_bytes = recv(0, buffer, sizeof(buffer), 0);

    if (strcmp(buffer, "pass") == 0)
        print_msg("The role has  been passed", -1, 1);
    else {
        print_msg("New answer: ", -1, 0);
        print_msg(buffer, num_of_bytes, 1);
    }
}


int main(int argc, char *argv[]) {
    int port = atoi(argv[1]);
    if (port <= 0)
        print_err_and_quit("Invalid port number given. Quitting ");

    siginterrupt(SIGALRM, 1);


    char buffer[2048] = {0};
    char print_buffer[1024];
    int fd;
    int num_of_bytes = 0;

    fd = connect_server(port);
    print_successs_msg("Connection to server has been established.");

    // receive the assigned ID from the server and print it
    num_of_bytes = recv(fd, buffer, sizeof(buffer), 0);
    int id = atoi(buffer);
    sprintf(buffer, "Your new id: %d", id);
    print_successs_msg(buffer);

    // receive the introduction message from the server and print it
    num_of_bytes = recv(fd, buffer, sizeof(buffer), 0);
    print_msg(buffer, num_of_bytes, 1);

    // Read field choice from user (user response) and send it to server
    read(0, buffer, sizeof(buffer));
    send(fd, buffer, strlen(buffer), 0);

    // Read the port number associated with the new broadcast connection
    num_of_bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (num_of_bytes <= 0)
        print_err_and_quit("Failed to fetch broadcast port number from the server.");
    int bc_port = atoi(buffer);
    sprintf(buffer, "Broadcast port is announced: %d", bc_port);
    print_successs_msg(buffer);

    // New broadcast socket
    int bc_fd = get_broadcast_socket(bc_port);
    if (bc_fd < 0)
        print_err_and_quit("Could not open broadcast socket.");
    print_successs_msg("Broadcast socket established.");

    for (int i = 0; i < 3; i++) {
        // Receive asking id ID from the server and print it
        num_of_bytes = recv(fd, buffer, sizeof(buffer), 0);
        int asking_id = atoi(buffer);
        sprintf(buffer, "It is client%d's role to ask.", asking_id);
        print_msg(buffer, -1, 1);

        // Get user question
        if (asking_id == id)
            ask_question(bc_fd);
        else if (abs(asking_id - id) == 1) {
            wait_for_question(bc_fd);
            ask_question(bc_fd);
            wait_for_response(bc_fd);
        } else {
            wait_for_question(bc_fd);
            wait_for_response(bc_fd);
            ask_question(bc_fd);
        }
    }
    return 0;
}