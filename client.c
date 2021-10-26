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
#define BROADCAST_ADDR0 "255.255.255.255"
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

int get_broadcast_socket(int port, struct sockaddr_in *address) {
    int fd;
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address->sin_family = AF_INET;
    address->sin_port = htons(port);
    address->sin_addr.s_addr = inet_addr(BROADCAST_ADDR0);

    if (bind(fd, (struct sockaddr *) address, sizeof(*address)) < 0)
        print_err_and_quit("Failed to bind broadcast socket.");
    return fd;
}

void udp_send(int bc_fd, char *buffer, struct sockaddr_in *address, int id) {
    char id_str[100];
    sprintf(id_str, "%d ", id);
    char *new_buffer = strcat(id_str, buffer);
    sendto(bc_fd, new_buffer, strlen(new_buffer), 0, (struct sockaddr *) address, sizeof(*address));
}

void ask_question(int bc_fd, struct sockaddr_in *address, int id) {
    char buffer[1024];

    print_msg("Please ask your question: \n", -1, 0);
    if (read(0, buffer, sizeof(buffer)) >= 0)
        udp_send(bc_fd, buffer, address, id);
}

void wait_for_question(int bc_fd, int id) {
    char buffer[1024];

    int sending_id;
    char new_buffer[1024];
    int num_of_bytes = recv(bc_fd, buffer, sizeof(buffer), 0);

    if (sscanf(buffer, "%d %s", &sending_id, new_buffer) <= 0)
        print_err_and_quit("Could not decode peer message.");

    if (id == sending_id)
        num_of_bytes = recv(bc_fd, buffer, sizeof(buffer), 0);

    if (sscanf(buffer, "%d %s", &sending_id, new_buffer) <= 0)
        print_err_and_quit("Could not decode peer message.");
    print_msg("New question: ", -1, 0);
    print_msg(new_buffer, -1, 1);
}

void answer_question(int bc_fd, struct sockaddr_in *address, int id) {
    char buffer[1024];
    print_msg("Please write your answer: ", -1, 1);
    if (read(0, buffer, sizeof(buffer)) >= 0)
        udp_send(bc_fd, buffer, address, id);
}

char *wait_for_response(int bc_fd, int id) {
    char buffer[1024];
    print_successs_msg("Awaiting response from the other client...");
    int num_of_bytes = recv(bc_fd, buffer, sizeof(buffer), 0);

    int sending_id;
    char new_buffer[1024];
    if (sscanf(buffer, "%d %s", &sending_id, new_buffer) <= 0)
        print_err_and_quit("Could not decode peer message.");
    if (id == sending_id)
        num_of_bytes = recv(bc_fd, buffer, sizeof(buffer), 0);

    if (sscanf(buffer, "%d %s", &sending_id, new_buffer) <= 0)
        print_err_and_quit("Could not decode peer message.");

    if (strcmp(new_buffer, "pass") == 0)
        print_successs_msg("Last role has  been passed.");
    else {
        print_msg("New answer: ", -1, 0);
        print_msg(new_buffer, -1, 1);
    }
    char *res = malloc(strlen(new_buffer) * sizeof(char));
    strcpy(res, new_buffer);
    return res;
}

void send_answers_back(char **answers, int best_answer, int server_fd) {
    if (best_answer < 0 || best_answer > 1)
        best_answer %= 2;

    char res[2148];
    if (best_answer == 0)
        sprintf(res, "* Answer1: %s\nAnswer2: %s\n", answers[0], answers[1]);
    else
        sprintf(res, "Answer1: %s\n* Answer2: %s\n", answers[0], answers[1]);

    send(server_fd, res, strlen(res), 0);
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
    printf("AAAAA: %d\n", num_of_bytes);
    print_msg(buffer, num_of_bytes, 1);

    // Read field choice from user (user response) and send it to server
    read(0, buffer, sizeof(buffer));
    send(fd, buffer, strlen(buffer), 0);

    // Read the port number associated with the new broadcast connection
    num_of_bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (num_of_bytes <= 0)
        print_err_and_quit("Failed to fetch broadcast port number from the server.");
    print_msg(buffer, num_of_bytes, 1);
    int bc_port = atoi(buffer);
    sprintf(buffer, "Broadcast port is announced: %d", bc_port);
    print_successs_msg(buffer);

    // New broadcast socket
    struct sockaddr_in address;
    int bc_fd = get_broadcast_socket(bc_port, &address);
    if (bc_fd < 0)
        print_err_and_quit("Could not open broadcast socket.");
    print_successs_msg("Broadcast socket established.");

    for (int i = 0; i < 3; i++) {
        // Receive asking id ID from the server and print it
        int answering_ids[2];
        int asking_id;

        buffer[0] = '\0';
        while(sscanf(buffer, "%d %d %d", &asking_id, &answering_ids[0], &answering_ids[1]) <= 0)
            num_of_bytes = recv(fd, buffer, sizeof(buffer), 0);

        sprintf(buffer, "It is client%d's role to ask.", asking_id);
        print_msg(buffer, -1, 1);
        // Get users' questions and answers
        if (asking_id == id) {
            char **answers = malloc(sizeof(char) * 2);
            ask_question(bc_fd, &address, id);

            char *answer = wait_for_response(bc_fd, id);

            answers[0] = malloc(sizeof(char) * strlen(answer));
            strcpy(answers[0], answer);
            answer = wait_for_response(bc_fd, id);

            answers[1] = malloc(sizeof(char) * strlen(answer));
            strcpy(answers[1], answer);
            print_msg("Please select the best answer: ", -1, 0);
            read(0, buffer, sizeof(buffer));
            send_answers_back(answers, atoi(buffer) - 1, fd);

            free(answers[0]);
            free(answers[1]);
            free(answers);
            free(answer);
        } else if (answering_ids[0] == id) {
            wait_for_question(bc_fd, id);

            answer_question(bc_fd, &address, id);

            wait_for_response(bc_fd, id);

        } else if (answering_ids[1] == id) {
            wait_for_question(bc_fd, id);

            wait_for_response(bc_fd, id);

            answer_question(bc_fd, &address, id);

        } else
            print_err_and_quit("Getting turn in Q&A failed.");
    }

    return 0;
}