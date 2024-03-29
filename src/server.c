#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "utils.h"

#define SA struct sockaddr
#define BUFF_SZ 320
#define MAX_CLIENTS 64

char buffer_inp_server[BUFF_SZ];
struct sockaddr_in cli;

void handle_leave_alert(endpoint_t client) {
    pthread_mutex_lock(&cth_lock);
    avail[client.client_n] = 0;
    close(clients[client.client_n].connfd);
    pthread_mutex_unlock(&cth_lock);
}

void handle_join_alert(endpoint_t client) {
    packet_t msg;
    construct_message(&msg, "\n [+]\n", 0, 2, client);

    printf("[+]");
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        send(clients[i].connfd, (packet_t*)&msg, sizeof(msg), 0);
    }
}

void* from_client(void* arg) {
    endpoint_t client = *(endpoint_t*)arg;
    int connfd = client.connfd;
    char buffer[BUFF_SZ];
    packet_t msg;
    FILE* fp;

    while (1) {
        
        bzero(buffer, sizeof(buffer));

        // waitin to recieve mesasge from the client
        if (recv(connfd, (packet_t*)&msg, sizeof(msg), 0) == 0) {
            /* checking if the client sends nothing back,
             * then it will terminate the connection
             */
            fflush(stdout);
            handle_leave_alert(client);
            close(connfd);
            return NULL;
        }

        char* current_time = get_current_time();
        switch (msg.type) {
            // handle normal messages
            case 0:
                for (int i = 0; i < MAX_CLIENTS; ++i) {
                    if (i == client.client_n || avail[i] == 0) {
                        continue;
                    }
                    if (send(clients[i].connfd, (packet_t*)&msg, sizeof(msg), 0) == 0) {
                        printf("error sending\n");
                    }
                }

                // clearing screen and printing
                printf("\33[2k\r");
                printf("[%s] <%s> %s\n", current_time, client.nickname, msg.input);
                printf("<host> %s", buffer_inp_server);
                fflush(stdout);
                break;
            case 1:
                break;
            case 2:
                break;
            case 3:
                // handle dm
                for (int i = 0; i < MAX_CLIENTS; ++i) {
                    if (strcmp(clients[i].nickname, msg.nickname_reciever) == 0) {
                        if (send(clients[i].connfd, (packet_t*)&msg, sizeof(msg), 0) == 0) {
                            printf("error sending\n");
                        }
                    }
                }

                // clearing screen and printing
                printf("\33[2k\r");
                printf("[%s] <DM> <%s> %s\n", current_time, client.nickname, msg.input);
                printf("<host> %s", buffer_inp_server);
                fflush(stdout);
                break;
            case 4:
                // handling server updates
                break;
            case 5:
                break;
        }
    }

    fclose(fp);
    close(connfd);
    return NULL;
}

void* start_server(void* arg) {
    endpoint_t client = *(endpoint_t*)arg;
    int connfd = client.connfd;

    // sending client info to client
    packet_t msg;
    msg.id_reciever = client.id;
    msg.type = 1;
    msg.client = client;
    if (send(connfd, (packet_t*)&msg, sizeof(msg), 0) == 0) {
        return NULL;
    }
    
    // receiving client back with added nickname
    if (recv(connfd, (packet_t*)&msg, sizeof(msg), 0) == 0) {
        return NULL;
    }

    /* code for checking if nickname is taken coming here
     *
     *
     *
     *
     * */

    strcpy(client.nickname, msg.client.nickname);
    strcpy(clients[client.client_n].nickname, msg.client.nickname);

    // creating thread to await messages from client
    pthread_t thid;
    pthread_create(&thid, NULL, from_client, &client);

    /* making sure the input field in the terminal is not blocket
     * by other processes running (from_client())
     */
    setNonBlockingInput();
    handle_join_alert(client);

    int n;
    for (;;) {
        n = 0;
        printf("\n<host> ");
        fflush(stdout);

        // getting input
        while (1) {

            char c = getchar();
            if (c == '\n' || c == 0xffffffff) {
                break;
            } else if (c == 0x8 || c == 0x7F) {
                if (n > 0) {
                    printf("\b \b");
                    --n;
                }
            } else {
                putchar(c);
                buffer_inp_server[n++] = c;
            }
        }

        if (strcmp(buffer_inp_server, "/stop") == 0) {
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                close(clients[i].connfd);
            }

            close(connfd);
            return NULL;
        }
        else if (n > 0) {
            packet_t msg;
            construct_message(&msg, buffer_inp_server, 0, 1, (endpoint_t)client); 
            if (send(connfd, (void*)&msg, sizeof(msg), 0) == 0) {
                return NULL;
            }

            bzero(buffer_inp_server, sizeof(buffer_inp_server));
        }
    }

    return NULL;
}

