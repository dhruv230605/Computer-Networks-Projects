#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define SERVER_PORT 5432
#define MAX_PENDING 5
#define MAX_LINE 256
#define MAX_CLIENTS 10 //upto 10 clients for now ig, maybe lower to 4?

int client_socks[MAX_CLIENTS]; //slots for connected client sockets
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; //for lockin

//to send server messages to a random client
void *server_input_thread(void *arg) {
    char buf[MAX_LINE];
    srand(time(NULL));

    while (1) {
        if (fgets(buf, sizeof(buf), stdin) != NULL) {
            buf[MAX_LINE - 1] = '\0'; //just in case of no newline, removable
            pthread_mutex_lock(&clients_mutex);
            int sent = 0;
            for (int tries =0; tries<MAX_CLIENTS;tries++) {
                int idx = rand()%MAX_CLIENTS; //can just to it round robin 
                if (client_socks[idx]!= -1) {
                    if (send(client_socks[idx], buf,strlen(buf)+1, 0) < 0) {
                        perror("send error");
                        close(client_socks[idx]);
                        client_socks[idx] = -1;
                    } 
                    else {
                        sent = 1;
                    }
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            // pthread_mutex_unlock(mutex);

            if (!sent) printf("No active clients to send message.\n");
        }
    }
    return NULL;
}

//Thread to handle a client
void *client_handler(void *arg) {
    int sd = *(int *)arg;
    char buf[MAX_LINE];
    int bytes;

    while (1) {
        bytes = recv(sd, buf, sizeof(buf), 0);
        if (bytes <= 0) {
            printf("Client fd %d disconnected.\n", sd);
            close(sd);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_socks[i] == sd) {
                    client_socks[i] = -1;
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            pthread_exit(NULL);
        } else {
            buf[MAX_LINE - 1] = '\0';
            printf("Client %d says: %s\n", sd, buf);
        }
    }
}

int main() {
    int listener;
    struct sockaddr_in sin;
    pthread_t tid;

    //client sockets setting all to -1 to start 
    for (int i = 0; i < MAX_CLIENTS; i++) client_socks[i] = -1;

    //Setup server
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(SERVER_PORT);

    if ((listener = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed"); exit(1);
    }
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("bind failed"); exit(1);
    }
    // if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) <= 0) {
    //     perror("bind failed"); exit(1);
    // }
    if (listen(listener, MAX_PENDING) < 0) {
        perror("listen failed"); exit(1);
    }

    printf("Threaded server listening on port %d...\n", SERVER_PORT);

    // Start server input thread
    pthread_create(&tid, NULL, server_input_thread, NULL);
    pthread_detach(tid);

    // Accept clients
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int new_sd = accept(listener, (struct sockaddr *)&client_addr, &addr_len);
        if (new_sd < 0) { perror("accept"); continue; }

        pthread_mutex_lock(&clients_mutex);
        int added = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_socks[i] == -1) {
                client_socks[i] = new_sd;
                added = 1;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);

        if (!added) {
            printf("Max clients reached. Rejecting client fd %d\n", new_sd);
            close(new_sd);
            continue;
        }

        printf("New client connected: fd %d\n", new_sd);
        pthread_create(&tid, NULL, client_handler, (void *)&new_sd);
        pthread_detach(tid);
    }

    close(listener);
    return 0;
}
