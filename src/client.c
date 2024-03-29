#include <arpa/inet.h>
#include <time.h>
#include <stdint.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include "utils.h"

#define SA struct sockaddr
#define BUFF_SZ 320 

char buffer_inp_client[BUFF_SZ];
short client_id;
char nickname[32];

void* from_server(void* arg) {
    int sockfd = *(int*)arg;
    char buffer[BUFF_SZ];
    packet_t msg;
    while (1) {
        bzero(buffer, sizeof(buffer));

        // waiting to recieve message from server
        if (recv(sockfd, (packet_t*)&msg, sizeof(msg), 0) == 0) return NULL;

        char* current_time = get_current_time();
        switch (msg.type) {
            case 0:
                // handle normal messages
                printf("\33[2K\r"); // deleting current line in terminal
                fflush(stdout); 
                printf("[%s] <%s> %s\n", current_time, msg.client.nickname, msg.input); // printing out message and sender
                printf("<%s> %s", nickname, buffer_inp_client);
                fflush(stdout);
                break;
            case 1:
                // handling manuel messages from host
                printf("\33[2K\r"); 
                fflush(stdout); 
                printf("<host> %s\n", msg.input); // printing out message and sender
                printf("<%s> %s", nickname, buffer_inp_client);
                fflush(stdout);
                break;
            case 2:
                // handling join/leave alerts (removed)
                printf("\33[2K\r"); // deleting current line in terminal
                fflush(stdout); 
                printf("%s\n", msg.input); 
                printf("<%s> %s", nickname, buffer_inp_client);
                fflush(stdout);
                break;
            case 3:
                printf("\33[2K\r"); // deleting current line in terminal
                fflush(stdout); 
                printf("\33[0;32m[%s] <%s>\33[0m %s\n", current_time, msg.client.nickname, msg.input); 
                printf("<%s> %s", nickname, buffer_inp_client);
                fflush(stdout);
                break;
            case 4:
                // handling server updates
                break;
            case 5:
                break;
        }
    }

    return NULL;
}

int start_client(int sockfd) {
    packet_t msg;
    pthread_t thid;

    // waiting to recieve info about the client itself (id)
    if(recv(sockfd, (packet_t*)&msg, sizeof(msg), 0) == 0) {
        return 1;
    }

    printf("nickname: ");
    fflush(stdout);
    fgets(nickname, sizeof(nickname), stdin);
    for (int i = 0; i < sizeof(nickname); ++i) {
        if (nickname[i] == '\n') {
            nickname[i] = '\0';
        }
    }

    strcpy(msg.client.nickname, nickname);
    if (send(sockfd, (packet_t*)&msg, sizeof(msg), 0) == 0) {
        return 0;
    }

    endpoint_t client = msg.client;
    client_id = msg.id_reciever;

    // creating thread to handle incoming messages
    pthread_create(&thid, NULL, &from_server, &sockfd);
    setNonBlockingInput();
    srand(time(0));

    int n;
    for (;;) {
        n = 0;
        printf("\n<%s> ", client.nickname);
        fflush(stdout);

        while (1) {
            // getting input
            char c = getchar();
            // checking if input is newline or NULL
            if (c == '\n' || c == 0xffffffff) {
                break;
                // chekcing if input is esc og backspace
            } else if (c == 0x8 || c == 0x7F) { 
                if (n > 0) {
                    // printing sequence to autobackspace
                    printf("\b \b"); 
                    --n;
                }
            } else {
                putchar(c); 
                buffer_inp_client[n++] = c;
            }
        }
        
        if (buffer_inp_client[0] == '/') {
            int count;
            char** args = split_string(buffer_inp_client, ' ', &count);
            if (strcmp(args[0], "/dm") == 0) {
                char dm_input[320];
                int idx = 0;
                for (int i = 2; i < count; ++i) {
                    for (int j = 0; j < strlen(args[i]); ++j) {
                       dm_input[idx++] = args[i][j];
                    }
                    if (idx != count - 1) {
                        dm_input[idx++] = ' ';
                    }
                }

                construct_message(&msg, dm_input, client_id, 3, (endpoint_t )client);
                strcpy(msg.nickname_reciever, args[1]);
                if (send(sockfd, (packet_t*)&msg, sizeof(msg), 0) == 0) {
                    printf("could not send DM.\n");
                }
            }

            free_tokens(args, count);
        }
        else if (n > 0) {
            // constructing and sending message
            construct_message(&msg, buffer_inp_client, client_id, 0, (endpoint_t )client);
            send(sockfd, (void*)&msg, sizeof(msg), 0);
        }

        bzero(buffer_inp_client, sizeof(buffer_inp_client));
    }

    return 0;
}

