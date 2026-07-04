#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "server.h"

// server received a string message from client_id
// string message is in msg
// len is the length of the message
// num_clients is maximum number of clients
// valid_ids is an integer array of size num_clients
// all integers between 0 to num_clients-1 are potential clients
// a client X is valid if valid_ids[i] != -1



#define MAX_RECIPIENTS 64

void recv_message(char *msg, int len, int client_id, int *valid_ids, int num_clients) {

    msg[strcspn(msg, "\n")] = '\0';

    //list
    if (strncmp(msg, "LIST", 4) == 0) {
        char buffer[512];
        int in = 0;
        in += sprintf(buffer, "valid client ids: ");
        for (int i =0; i <num_clients; i++) {   // we traverse the clients 
            if (valid_ids[i] != INVALID_ID && i != client_id) {   // only valid tho 
                in +=sprintf(buffer + in,"%d ", i);         // cant be itelf hah thought i wouldnt notice
            }
        }
        send_message(buffer, strlen(buffer), client_id, SERVER_ID);
        return;
    }

    //data
    if (strncmp(msg, "DATA", 4) == 0) {
        int n;
        int ids[MAX_RECIPIENTS];
        char body[512] = {0};

        char *colon = strchr(msg, ':');
        char header[256] = {0};

        if (colon){       // colon just gotta be there man before colon header rest body 
            strncpy(header, msg, colon - msg);
            header[colon - msg] = '\0';
            char *body_start = colon + 1;
            strcpy(body, body_start);
        } 

        char *token = strtok(header, " ");
        token = strtok(NULL, " "); // n
        if (!token) {
             printf("what are u sending holy pls %d\n", client_id);
             return;
         }

        n = atoi(token);

        for (int i =0; i < n;i++) {
            token = strtok(NULL, " ");
            if (!token) break;
            ids[i] = atoi(token);
        }

        for (int i=0; i <n; i++) {
            int dst = ids[i];             // we go again 
            if (dst >= 0 && dst < num_clients && valid_ids[dst] != INVALID_ID) {      //only send valid and the ones client wants to sends to 
                send_message(body, strlen(body), dst, client_id);
            } 
            else {
                printf("where you sedning that. dst id %d to client %d wrong ash\n", dst, client_id);
            }
        }

        printf("client %d: %s\n", client_id, msg);
    }
}
