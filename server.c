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

typedef struct {
    int list[3];
    int len;
} field_list;

typedef struct {
    int fds[3];
    char messages[3][4096];
    int msg_count;
} room;

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

int init_server(int port) {
    struct sockaddr_in address;
    int server_fd;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0)
        print_err_and_quit("Could not bind address to socket.");

    listen(server_fd, INT_MAX);
    return server_fd;
}

int acceptClient(int serverFd) {
    int client_fd;
    struct sockaddr_in c_addr;

    int addressLen = sizeof(c_addr);
    client_fd = accept(serverFd, (struct sockaddr *) &c_addr, (socklen_t *) &addressLen);

    return client_fd;
}

room *
add_client_to_specific_group_buffer(field_list *comp, field_list *elec, field_list *mech, field_list *civil, int client,
                                    int request) {
    field_list *curr_list;
    switch (request) {
        case 2:
            curr_list = elec;
            break;
        case 3:
            curr_list = mech;
            break;
        case 4:
            curr_list = civil;
            break;
        default:
            curr_list = comp;
    }

    if (curr_list->len < 2){
        curr_list->list[curr_list->len] = client;
        curr_list->len++;
    }else {
        room *new_room;
        new_room = malloc(sizeof(room));
        new_room->msg_count = 0;
        for (int i = 0; i < 2; i++)
            new_room->fds[i] = curr_list->list[i];
        new_room->fds[2] = client;

        curr_list->len = 0;
        return new_room;
    }
    return NULL;
}

void assign_broadcast_port(room r, int port, fd_set* rooms_set) {
    char buffer[1024];
    sprintf(buffer, "New broadcast port is assigned: %d\n", port);
    for (int i = 0; i < 3; i++) {
        send(r.fds[i], buffer, strlen(buffer), 0);
    }

    sprintf(buffer, "Please write your summary: \n");
    for (int i = 0; i < 3; i++) {
        FD_SET(r.fds[i], rooms_set);
        send(r.fds[i], buffer, strlen(buffer), 0);
    }
}

room *find_room(room **rooms, int rooms_len, int client, int *client_index) {
    for (int i = 0; i < rooms_len; i++) {
        for (int j = 0; j < 3; j++) {
            if (rooms[i]->fds[j] == client) {
                *client_index = j;
                return rooms[i];
            }
        }
    }
    return NULL;
}

void write_messages_to_file(room r) {
    int f = open("messages.txt", O_APPEND);
    for (int i = 0; i < 3; i++) {
        write(f, r.messages[i], strlen(r.messages[i]));
    }
}

void close_room(room r, fd_set *rooms_set, fd_set *master_set) {
    write_messages_to_file(r);

    char *buffer = "The room has been closed.";
    for (int i = 0; i < 3; i++) {
        send(r.fds[i], buffer, strlen(buffer), 0);
        FD_CLR(i, rooms_set);
        FD_CLR(i, master_set);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2)
        print_err_and_quit("Could not read port from calling input. Quitting...");

    int port = atoi(argv[1]);
    if (port <= 0)
        print_err_and_quit("Invalid port number given. Quitting ");

    int server_fd, new_socket;
    char buffer[2048] = {0};
    char print_buffer[1024];
    fd_set master_set, working_set, rooms_set;
    room **rooms;
    int last_assigned_port = BASE_ASSIGNED_BROADCAST_PORT - 1, rooms_len = 0;

    server_fd = init_server(port);
    int max_fd_id = server_fd;

    FD_ZERO(&master_set);
    FD_ZERO(&rooms_set);
    max_fd_id = server_fd;
    FD_SET(server_fd, &master_set);

    sprintf(print_buffer, "Server currently listening at %s:%d", LOOPBACK_ADDR, port);
    print_successs_msg(print_buffer);

    field_list comp;
    field_list elec;
    field_list mech;
    field_list civil;

    while (1) {
        working_set = master_set;
        select(max_fd_id + 1, &working_set, NULL, NULL, NULL);

        for (int i = 0; i <= max_fd_id; i++) {
            if (FD_ISSET(i, &working_set)) {
                if (i == server_fd) {
                    new_socket = acceptClient(server_fd);
                    FD_SET(new_socket, &master_set);
                    if (new_socket > max_fd_id)
                        max_fd_id = new_socket;
                    sprintf(print_buffer, "New client with ID=%d connected", new_socket);
                    print_successs_msg(print_buffer);

                    sprintf(buffer,
                            "Please select a field:\n\t[1]Computer Engineering\n\t[2]Electrical Engineering\n\t[3]Mechanics Engineering[4]\n\tDefualt: 1\n");
                    send(new_socket, buffer, strlen(buffer), 0);
                } else {
                    if (FD_ISSET(i, &rooms_set)) {
                        if (recv(i, buffer, 2048, 0) == 0) {
                            close(i);
                            FD_CLR(i, &master_set);

                            sprintf(print_buffer, "Closed connection to client with ID=%d", i);
                            print_successs_msg(print_buffer);
                        }

                        int client_index = -1;
                        room *r = find_room(rooms, rooms_len, i, &client_index);
                        if (r == NULL) {
                            continue;
                        }
                        strcpy(r->messages[client_index], buffer);
                        r->msg_count++;
                        if (r->msg_count == 3)
                            close_room(*r, &rooms_set, &master_set);
                    } else {
                        if (recv(i, buffer, 2048, 0) == 0) {
                            close(i);
                            FD_CLR(i, &master_set);

                            sprintf(print_buffer, "Closed connection to client with ID=%d", i);
                            print_successs_msg(print_buffer);
                        }
                        // Reading input from a client into the buffer has been successful
                        int client_choice = atoi(buffer);

                        // Set the default choice
                        if (client_choice <= 0 || client_choice > 4)
                            client_choice = 1;

                        room *new_room = add_client_to_specific_group_buffer(&comp, &elec, &mech, &civil, i,
                                                                             client_choice);
                        if (new_room != NULL) {
                            assign_broadcast_port(*new_room, ++last_assigned_port, &rooms_set);
                            rooms = realloc(rooms, (rooms_len + 1) * sizeof(room));
                            rooms[rooms_len] = new_room;
                            rooms_len++;
                        }
                    }
                }
            }

        }
    }
    return 0;
}



/*
while (1) {
working_set = master_set;
select(max_fd_id + 1, &working_set, NULL, NULL, NULL);

for (int i = 0; i < max_fd_id; i++) {
if (FD_ISSET(i, &working_set)) {
if (i == server_fd) {
new_socket = acceptClient(server_fd);
FD_SET(new_socket, &master_set);
if (new_socket > max_fd_id)
max_fd_id = new_socket;
sprintf(print_buffer, "New client with ID=%d connected", new_socket);
print_successs_msg(print_buffer);
} else {
if(recv(i, buffer, strlen(buffer), 0)==0){
close(i);
FD_CLR(i, &master_set);

sprintf(print_buffer, "Closed connection to client with ID=%d", i);
print_successs_msg(print_buffer);
}

// Reading input from a client into the buffer has been successful



}


}
}

}*/
