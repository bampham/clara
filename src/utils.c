#include <arpa/inet.h> 
#include <linux/limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> 
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h> 
#include <termios.h>
#include <pthread.h>
#include <time.h>

#include "utils.h"

#define SA struct sockaddr
#define MAX_CLIENTS 64

// struct for arguemnt passed to a thread
typedef struct {
    int connfd;
} ThreadArgs;

endpoint_t clients[64]; // array of clients
int avail[64]; // array of available clients
pthread_mutex_t cth_lock; // variable for locking mutex when accessing global data

// non blocking function manipulating bits in tty to ensure the tty state
void setNonBlockingInput() {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~ICANON;
    ttystate.c_lflag &= ~ECHO; 
    ttystate.c_cc[VMIN] = 1;
    ttystate.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

/* 
 * It constructs a message with the input.
 * Type:
 *      0 -> Normal chatting message
 *      1 -> Manuel message from the host
 *      2 -> Join/leave alert
 *      3 -> Direct Message
 *      4 -> Server updates (like restart and going offline)
 *      5 -> Commands/requests
 */
void construct_message(packet_t* msg, char* input, int id_sender, int type, endpoint_t client) {
    strcpy(msg->input, input);
    msg->id_sender = id_sender;
    msg->type = type;
    msg->client = client;
}


char* get_current_time() {
    time_t time_seconds = time(NULL);
    char* current_time = (char*)malloc(16);
    current_time = ctime(&time_seconds);
    current_time[strlen(current_time)-1] = '\0';
    return current_time;
}

void free_tokens(char** tokens, int count) {
    if (tokens == NULL) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        free(tokens[i]);
    }

    free(tokens);
}

char** split_string(const char* str, char splitter, int* count) {
    int token_n = 0;
    const char* temp = str;
    
    while (*temp != '\0') {
        if (*temp == splitter) {
            ++token_n;
        }
        ++temp;
    }
    ++token_n;  
    
    char** tokens = (char**)malloc(token_n * sizeof(char*));
    if (!tokens) {
        *count = 0;
        return NULL;
    }
    
    int token_idx = 0;
    char* token = strtok((char*)str, &splitter);
    while (token != NULL) {
        tokens[token_idx] = (char*)malloc(320 * sizeof(char));
        if (!tokens[token_idx]) {
            for (int i = 0; i < token_idx; ++i) {
                free(tokens[i]);
            }
            free(tokens);
            *count = 0;
            return NULL;
        }
        strcpy(tokens[token_idx], token);
        ++token_idx;
        token = strtok(NULL, &splitter);
    }
    
    *count = token_n;
    return tokens;
}


