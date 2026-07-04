#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#define SERVER_PORT  5432
#define MAX_PENDING  5
#define MAX_LINE     256
#define MAX_CLIENTS  10   //upto 10 clients for now ig, maybe lower to 4?

int main() {
    struct sockaddr_in sin;
    int listener;                 
    int client_socks[MAX_CLIENTS]; //for connected client fds, -1 if empty
    int max_fd, activity; //maxfd for the highest one for tracking
    fd_set readfds;
    char buf[MAX_LINE];
    int i;

    //clear client socket list
    for (i = 0;i< MAX_CLIENTS; i++) {
        client_socks[i] = -1;
    }
    // for (auto i : client_socks){
    //     i = -1;
    // }

    //building address data structure
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);

    //create socket
    if ((listener = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(1);
    }
    // if ((listener = socket(PF_INET, SOCK_STREAM, 0)) <= 0) {
    //     perror("socket failed");
    //     exit(1);
    // }

    // allow port reuse
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //binding
    if ((bind(listener, (struct sockaddr *)&sin, sizeof(sin))) < 0) {
        perror("bind failed");
        exit(1);
    }

    // listening
    if (listen(listener, MAX_PENDING) <0) {
        perror("listen failed");
        exit(1);
    }

    printf("Server listening on port %d..\n", SERVER_PORT);

    // main loop
    while (1) {
        FD_ZERO(&readfds);

        // addign stdin
        FD_SET(STDIN_FILENO, &readfds);
        max_fd = STDIN_FILENO;

        // adding listening socket
        FD_SET(listener, &readfds);
        if (listener > max_fd) max_fd = listener;

        // adding all active client sockets
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (client_socks[i] != -1) {
                FD_SET(client_socks[i], &readfds);
                if (client_socks[i] > max_fd) {
                    max_fd = client_socks[i];
                }
            }
        }

        // wait for activity
        activity = select(max_fd +1, &readfds, NULL, NULL, NULL);
        if (activity<0) {
            perror("select");
            break;
        }
        // check if new connection
        if (FD_ISSET(listener, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_s = accept(listener, (struct sockaddr *)&client_addr, &addr_len);
            if (new_s < 0) {
                perror("accept");
            } else {
                printf("New client connected: fd %d\n", new_s);
                // store in first free slot
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (client_socks[i] == -1) {
                        client_socks[i] = new_s;
                        break;
                    }
                }
            }
        }

        // check if server typed something
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buf, sizeof(buf), stdin) != NULL) {
                buf[MAX_LINE - 1] = '\0';
                //sending to the first active client we can measily make it round robin if wanted by omplementing counter and modulus
                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (client_socks[i] != -1) {
                        if (send(client_socks[i], buf, strlen(buf) + 1, 0) < 0) {
                            perror("send error");
                            close(client_socks[i]);
                            client_socks[i] = -1;
                        }
                        break; //only one client
                    }
                }
            }
        }

        // check all clients for incoming messages
        for (i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_socks[i];
            if (sd != -1 && FD_ISSET(sd, &readfds)) {
                int bytes = recv(sd, buf, sizeof(buf), 0);
                buf[MAX_LINE - 1] = '\0';
                if (bytes <= 0) {
                    printf("Client fd %d disconnected.\n", sd);
                    close(sd);
                    client_socks[i] = -1;
                } else {
                    printf("Client %d says: %s\n", sd, buf);
                }
            }
        }
    }

    close(listener);
    return 0;
}
