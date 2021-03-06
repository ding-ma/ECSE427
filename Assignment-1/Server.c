#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include "a1_lib.h"

#define BUFSIZE   2024
#define NUMBER_CLIENT 15

int addInts(char *cmd) {
    char *token = strtok(cmd, " ");
    int x = atoi(token);
    token = strtok(NULL, " ");
    int y = atoi(token);
    
    return x + y;
}

int multiplyInts(char *cmd) {
    char *token = strtok(cmd, " ");
    int x = atoi(token);
    token = strtok(NULL, " ");
    int y = atoi(token);
    
    return x * y;
}

int isValidDivition(char *cmd) {
    char *token = strtok(cmd, " ");
    float x = atof(token);
    token = strtok(NULL, " ");
    float y = atof(token);
    
    return y != 0.0;
}

float divideFloats(char *cmd) {
    char *token = strtok(cmd, " ");
    float x = atof(token);
    token = strtok(NULL, " ");
    float y = atof(token);
    
    return x / y;
}

void sleeps(char *cmd) {
    int x = atoi(cmd);
    sleep(x);
}

uint64_t factHelper(int cur, uint64_t acc) {
    if (cur <= 1) {
        return acc;
    } else {
        return factHelper(cur - 1, acc * cur);
    }
}

uint64_t factorial(char *cmd) {
    int x = atoi(cmd);
    return factHelper(x, 1);
}


int isCommandValid(char *cmd) {
    char *commands[] = {"add", "multiply", "divide", "sleep", "factorial", "exit\n", "shutdown\n"};
    int len = sizeof(commands) / sizeof(commands[0]);
    for (int i = 0; i < len; ++i) {
        if (strcmp(commands[i], cmd) == 0) {
            return i + 1;
        }
    }
    return 0;
}

int isInputLengthValid(char *cmd, int expectedLength) {
    char *token = strtok(cmd, " ");
    int curr = 0;
    while (token != NULL) {
        token = strtok(NULL, " ");
        curr++;
    }
    return curr == expectedLength;
}

int isAllDone(int *arr) {
    for (int i = 1; i < NUMBER_CLIENT; ++i) {
        if (arr[i] != 0) {
            return 0;
        }
    }
    return 1;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        return EXIT_FAILURE;
    }
    
    int *processes = mmap(NULL, NUMBER_CLIENT * sizeof(int),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS,
                          0, 0);
    
    if (processes == MAP_FAILED) {
        printf("Mapping Failed\n");
        return 1;
    }
    // for the signals 1 means busy, 0 means idle
    processes[0] = 1; //we dont shutdown
    int processCounter = 1; //first one is reserved for shutdown signal
    
    int socketFd, clientFd;
    char msg[BUFSIZE];
    char msgCopy[BUFSIZE];
    char answer[BUFSIZE];
    message *sentMsg;
    
    if (create_server(argv[1], atoi(argv[2]), &socketFd) < 0) {
        perror("listen socket create error\n");
        return EXIT_FAILURE;
    }
//    freopen("/dev/null", "w", stderr); //surpress debug mode
    fprintf(stderr, "Server listening on %s:%s \n", argv[1], argv[2]);
    
    int processChecker = fork(); //have a process to pool the shared memory in order to check if all programs are done when calling shutdown
    if (processChecker == 0) {
        int cycle = 0;
        while (1) {
            if (processes[0] == 0) {
                close(socketFd);
                fprintf(stderr, "prearing shutdown, waiting on process to finish \n");
                if (isAllDone(processes)) {
                    kill(0, SIGTERM);
                    free(processes);
                    break;
                }
    
            }
            
            fprintf(stderr, "%d - ", cycle++);
            for (int i = 0; i < NUMBER_CLIENT; ++i) {
                fprintf(stderr, "c%d, s%d | ", i, processes[i]);
            }
            printf("\n");
            usleep(250);
        }
    } else {
        //this is where we accept client calls
        while (1) {
            int socket = accept_connection(socketFd, &clientFd);
            fprintf(stderr, "client accepted\n");
            if (socket < 0) {
                perror("listen socket bind error\n");
                return EXIT_FAILURE;
            }
            
            int pid = fork();
            if (pid == 0 && processes[0]) {
                close(socketFd); //dont need parent socket
                while (processes[0]) {
                    memset(msg, 0, sizeof(msg));
            
                    ssize_t byteCount = recv_message(clientFd, msg, BUFSIZE);
                    processes[processCounter] = 1;
            
                    sentMsg = (message *) msg;
                    dup2(clientFd, fileno(stdout));
                    fprintf(stderr, "counter %d \n", processCounter);
                    strcpy(msgCopy, sentMsg->arguments);
                    int result = isCommandValid(sentMsg->function);
            
                    if (result == 1) {
                        if (isInputLengthValid(sentMsg->arguments, 3)) {
                            sprintf(answer, "%d", addInts(msgCopy));
                        } else {
                            sprintf(answer, "add needs 2 numbers");
                        }
                    } else if (result == 2) {
                        if (isInputLengthValid(sentMsg->arguments, 3)) {
                            sprintf(answer, "%d", multiplyInts(msgCopy));
                        } else {
                            sprintf(answer, "multiple needs 2 numbers");
                        }
                    } else if (result == 3) {
                        if (isInputLengthValid(sentMsg->arguments, 3)) {
                            strcpy(msg, msgCopy);
                            if (isValidDivition(msgCopy)) {
                                sprintf(answer, "%f", divideFloats(msg));
                            } else {
                                sprintf(answer, "Error: Division by zero");
                            }
                        } else {
                            sprintf(answer, "divide needs 2 numbers");
                        }
                    } else if (result == 4) {
                        if (isInputLengthValid(sentMsg->arguments, 2)) {
                            sleeps(msgCopy);
                            sprintf(answer, " ");
                        } else {
                            sprintf(answer, "sleep needs 1 number");
                        }
                    } else if (result == 5) {
                        if (isInputLengthValid(sentMsg->arguments, 2)) {
                            sprintf(answer, "%lu", factorial(msgCopy));
                        } else {
                            sprintf(answer, "factorial needs 1 number");
                        }
                    } else if (result == 6) {
                        sprintf(answer, " ");
                        processes[processCounter] = 0;  // indicates client is done
                        close(clientFd);
                        return EXIT_SUCCESS;
                    } else if (result == 7) {
                        sprintf(answer, " ");
                        processes[0] = 0; //send shutdown signal
                        processes[processCounter] = 0; // indicates client is done
                        close(clientFd);
                        return EXIT_SUCCESS;
                    } else {
                        sprintf(answer, "Error: Command \"%s\" not found", sentMsg->function);
                    }
                    
                    if (byteCount <= 0) {
                        break;
                    }
                    send_message(clientFd, answer, strlen(answer));
                    // indicate end of command output
                    fflush(stdout);
                    processes[processCounter] = 0; // indicates client is done
                }
            } else {
                //close the socket for the parent as we dont need it
                close(clientFd);
            }
            processCounter++;
        }
    }
    close(socketFd);
}