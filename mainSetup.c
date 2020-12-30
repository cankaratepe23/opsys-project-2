#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <wait.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */

// Struct and head pointer for the linked list which is responsible for storing the background processes.
typedef struct bgProcNodeStruct {
    int index;
    int pid;
    char command[MAX_LINE];
    struct bgProcNodeStruct *next;
} BgProcNode;

BgProcNode *bgProcHead = NULL;

/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */

void setup(char inputBuffer[], char *args[], int *background) {
    int length, /* # of characters in the command line */
    i,          /* loop index for accessing inputBuffer array */
    start,      /* index where beginning of next command parameter is */
    ct;         /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ((length < 0) && (errno != EINTR)) {
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

    for (i = 0; i < length; i++) { /* examine every character in the inputBuffer */

        switch (inputBuffer[i]) {
            case ' ':
            case '\t' :               /* argument separators */
                if (start != -1) {
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */
                if (start != -1) {
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default :             /* some other character */
                if (start == -1)
                    start = i;
                if (inputBuffer[i] == '&') {
                    *background = 1;
                    inputBuffer[i - 1] = '\0';
                }
        } /* end of switch */
    }    /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */
} /* end of setup routine */


// Copies from dest node to src node, including only the essential parts.
void nodecpy(BgProcNode *dest, BgProcNode *src) {
    strcpy(dest->command, src->command);
    dest->index = src->index;
    dest->pid = src->pid;
}

// Adds a node to the main background processes linked list
void addBgProc(BgProcNode *node) {
    BgProcNode *curNode = bgProcHead;
    if (bgProcHead == NULL) {
        bgProcHead = malloc(sizeof(BgProcNode));
        nodecpy(bgProcHead, node);
        bgProcHead->next = NULL;
    } else {
        while (curNode->next != NULL) {
            curNode = curNode->next;
        }
        curNode->next = malloc(sizeof(BgProcNode));
        nodecpy(curNode->next, node);
        curNode->next->next = NULL;
    }
}

// Removes a node from the main background processes linked list
void removeBgProc(BgProcNode *node) {
    BgProcNode *prevNode = NULL;
    BgProcNode *curNode = bgProcHead;
    while (curNode != NULL) {
        if (curNode == node) {
            if (prevNode == NULL) {
                bgProcHead = curNode->next;
            } else {
                prevNode->next = curNode->next;
            }
            free(curNode);
            curNode = NULL;
            return;
        } else {
            prevNode = curNode;
            curNode = curNode->next;
        }
    }
}

// From the main background processes linked list, constructs two different linked lists, one for Running background processes and another for the ones which are finished.
// If a background process is added to the "finished" list, it is removed from the main linked list.
// All linked lists created here should be freed, as they are local.
void printBgProcList(BgProcNode *head) {
    BgProcNode *runningHead = NULL;
    BgProcNode *runningCur = NULL;
    BgProcNode *finishedHead = NULL;
    BgProcNode *finishedCur = NULL;
    BgProcNode *curNode = head;
    while (curNode != NULL) {

        /* TODO: DEBUG CODE DELETE LATER */
        int status = 0;
        pid_t returnVal = waitpid(curNode->pid, &status, WNOHANG);
        if (returnVal == -1) {
            printf("Error\n");
        } else if (returnVal == 0) {
            printf("Child %d is still running.\n", curNode->pid);
        } else if (WIFEXITED(status)) {
            printf("Child %d has exited.\n", curNode->pid);
        } else if (WIFSTOPPED(status)) {
            printf("Child %d has stopped.\n", curNode->pid);
        } else if (WIFSIGNALED(status)) {
            printf("Child %d has signaled.\n", curNode->pid);
        } else if (WIFCONTINUED(status)) {
            printf("Child %d has continued.\n", curNode->pid);
        }


        if (waitpid(curNode->pid, NULL, WNOHANG) != 0) {
            if (finishedHead == NULL) {
                finishedHead = malloc(sizeof(BgProcNode));
                finishedCur = finishedHead;
            } else if (finishedCur->next == NULL) {
                finishedCur->next = malloc(sizeof(BgProcNode));
                finishedCur = finishedCur->next;
            }
            nodecpy(finishedCur, curNode);
            removeBgProc(curNode);
            finishedCur->next = NULL;
        } else {
            if (runningHead == NULL) {
                runningHead = malloc(sizeof(BgProcNode));
                runningCur = runningHead;
            } else if (runningCur->next == NULL) {
                runningCur->next = malloc(sizeof(BgProcNode));
                runningCur = runningCur->next;
            }
            nodecpy(runningCur, curNode);
            runningCur->next = NULL;
        }
        curNode = curNode->next;
    }
    if (runningHead != NULL) {
        printf("Running:\n");
        curNode = runningHead;
        while (curNode != NULL) {
            if (curNode->pid != 0) {
                printf("\t[%d]%s (Pid=%d)\n", curNode->index, curNode->command, curNode->pid);
            }
            BgProcNode *freeCopy = curNode;
            curNode = curNode->next;
            free(freeCopy);
        }
    }
    if (finishedHead != NULL) {
        printf("Finished:\n");
        curNode = finishedHead;
        while (curNode != NULL) {
            if (curNode->pid != 0) {
                printf("\t[%d]%s (Pid=%d)\n", curNode->index, curNode->command, curNode->pid);
            }
            BgProcNode *freeCopy = curNode;
            curNode = curNode->next;
            free(freeCopy);
        }
    }
}

int is_in_directory(char *command,char *directory){
    DIR *d;
    struct dirent *dir;
    d = opendir(directory);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if(!strcmp(dir->d_name,command)){
                return 1;
            }
        }
        closedir(d);
    }
    return 0;
}

void free_2d_char(char **p,int rows){
    for(int i=0;i<rows;i++){
        free(p[i]);
    }
}

int get_executable_directory(char *directory,char *command) {
    char *path_env = getenv("PATH");
    char *path = (char*)malloc(sizeof(char)*strlen(path_env));
    strcpy(path, path_env);
    char *token;
    char delimiter[2] = ":";
    int path_length=0;
    for(int i=0;i<strlen(path);i++) {
        if (path[i] == ':') path_length++;
    }
    char *variables[path_length];
    token = strtok(path, delimiter);
    int path_index=0;
    while (token != NULL) {
        variables[path_index] = (char*)malloc(sizeof(char)*strlen(token));
        strcpy(variables[path_index],token);
        token = strtok(NULL, delimiter);
        path_index++;
    }
    free(path);
    for(int i=0;i<path_index;i++){
        if (is_in_directory(command,variables[i])){
            strcpy(directory,variables[i]);
            strcat(directory,"/");
            strcat(directory,command);
            free_2d_char(variables,path_index);
            return 1;
        }
    }
    return 0;
}


int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering of stdout to get proper console output.
    char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
    int background; /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE / 2 + 1]; /*command line arguments */
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop" // Ignore warnings for endless loop.
    while (1) {
        background = 0;
        printf("myshell: ");
        /*setup() calls exit() when Control-D is entered */
        setup(inputBuffer, args, &background);
        // Remove the '&' character from the arguments if it was included as an argument.
        if (background) {
            int argIndex = 0;
            while (args[argIndex][0] != '&') {
                argIndex++;
            }
            args[argIndex] = NULL;
        }

        // Convert the command/binary name to lowercase
        for (int i = 0; args[0][i]; i++) args[0][i] = tolower(args[0][i]);

        if (strcmp(args[0], "ps_all") == 0) {
            printBgProcList(bgProcHead);
        } else if (strcmp(args[0], "search") == 0) {
            // Search command goes here
        } else if (strcmp(args[0], "bookmark") == 0) {
            // Bookmark command goes here
        } else {
            char directory[50];
            int command_found_flag=0;
            if(get_executable_directory(directory,args[0])){
                command_found_flag=1;
            }
            pid_t pid = fork();
            if (pid == 0) {
                // This code block will be run as the child process.
                if(command_found_flag){
                    execv(directory, args);
                }else{
                    printf("Command not found.\n");
                }

            } else {
                // If process is not backgrounded, wait for it to exit. If it is backgrounded, add it to the background processes linked list.
                if (background == 0) {
                    waitpid(pid, NULL, 0);
                } else {
                    BgProcNode newBgProc;
                    newBgProc.pid = pid;
                    strcpy(newBgProc.command, inputBuffer);
                    addBgProc(&newBgProc);
                }
            }
        }
    }
#pragma clang diagnostic pop
}
