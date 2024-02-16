
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "parsecmd.h"

void sigchld_handler(int signum);
void handle_exit(char ** args, struct cmd * parsed_cmd);
void handle_cd(char ** args, char * home);
void handle_fork_no_pipe(struct cmd * parsed_cmd);
void handle_fork_with_pipe(struct cmd * parsed_cmd);

#define READ_END 0
#define WRITE_END 1
#define STD_IN 0
#define STD_OUT 1
#define STD_ERR 2


int main(int argc, char **argv) {
    char cmdline[MAXLINE];
    struct cmd * parsed_cmd;
    int bg = 0;
    int cmd_empty;

    // Add a call to signal to register SIGCHLD signal handler.
    signal(SIGCHLD, sigchld_handler);

    //Set home environment for "cd" command.
    char *home = getenv("HOME"); 

    while (1) {
        cmd_empty = 1;

        // Print the shell prompt
        printf("myunixshell> ");
        fflush(stdout);

        // Read in the next command entered by the user
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
            perror("fgets error");
        }
        if (feof(stdin)) { /* End of file (ctrl-D) */
            fflush(stdout);
            exit(0);
        }

        //Change flag to 1 if the cmdline is not all whitespace.
        //This will help handle empty commands.
        if (cmdline[0] != ' ' && cmdline[0] != '\n') {
            cmd_empty = 0;
        }

        if (cmd_empty == 0) {

            parsed_cmd = parse_cmd_dynamic(cmdline, &bg);

            
            handle_exit(parsed_cmd->cmd1, parsed_cmd);
            handle_cd(parsed_cmd->cmd1, home);
            
            if (parsed_cmd->has_pipe) {
                handle_fork_with_pipe(parsed_cmd);
                handle_cd(parsed_cmd->cmd2, home);
            } else {
                handle_fork_no_pipe(parsed_cmd);
            }

           

            free_cmd(parsed_cmd);
        }
    }
    return 0;
}

/*This function handles EXIT*/
void handle_exit(char ** args, struct cmd * parsed_cmd) {
    if (strcmp(args[0], "exit") == 0) {
        free_cmd(parsed_cmd);
        exit(0);
    }
}

/*This function handles CD*/
void handle_cd(char ** args, char * home) {
    //handling cd no path 
    if (strcmp(args[0], "cd")==0 && (args[1]==NULL)){
        if (home == NULL) {
            printf("'HOME' environment not found.");
            fflush(stdout);
        } else {
            // if cd without path, chdir to HOME
            if (chdir(home) != 0) {
                printf("Directory not found\n");
                fflush(stdout);
            }
        }
    }

    //handling cd with path
    if (strcmp(args[0], "cd")==0 && (args[1]!=NULL)){
        // if cd with path, chdir to args[1]
        if (chdir(args[1]) != 0){
            printf("Directory not found\n");
            fflush(stdout);
        }
    }
}

//Support function for FORKING
void exec_cmd(char ** args, char ** fds) {
    if (strcmp(args[0],"cd")==0) {
        exit(0);
    }

    /*This section handles I/O redirection */
    int fd_stdin, fd_stdout, fd_stderr;

    if (fds[STD_IN] != NULL) { // if there is stdin
        fd_stdin = open(fds[0], O_RDONLY);
        // printf("Created a stdin fd at %d.\n", fd_stdin);

        if (fd_stdin == -1) {
            perror("Invalid STDIN file descriptor.");
            exit(0);
        }

        if (dup2(fd_stdin, STD_IN) == -1) {
            perror("Dup2 failed for STDIN.");
            exit(0);
        };
    }

    if (fds[STD_OUT] != NULL) { // if there is stdout
        fd_stdout = open(fds[1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

        if (fd_stdout == -1) {
            perror("Invalid STDOUT file descriptor.");
            exit(0);
        }

        if (dup2(fd_stdout, STD_OUT) == -1) {
            perror("Dup2 failed for STDOUT.");
            exit(0);
        }
    }
    
    if (fds[STD_ERR] != NULL) { // if there is stderr
        fd_stderr = open(fds[2], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

        if (fd_stderr == -1) {
            perror("Invalid STDERR file descriptor.");
            exit(0);
        }
        
        if (dup2(fd_stderr, STD_ERR) == -1) {
            perror("Dup2 failed for STDERR.");
            exit(0);
        }
    }
    /*****************************************/


    // This section executes the program
    int flag = 0;
    if (args[0][0] == '/' || args[0][0] == '.' ) { //if this command is a specific path
        if (execv(args[0], args)!= 0) {
            flag = 0;
        } else {
            flag = 1;
        }
    } else {
        char * env = getenv("PATH"); 
        // printf("%s\n", env);
        if (env == NULL) {
            perror("No match in environment list.");
        }
        char * env_copy = strdup(env);
        char * path_ = malloc(sizeof(char) * 256);
        memset(path_, 0, sizeof(char) * 256);
        int l = 0, r = 1;
        
        while ( r < strlen(env_copy) ) {
            while(env_copy[r]!= '\0' && env_copy[r]!= ':') {
                r ++;
            }
            strncpy(path_, env_copy+l, r-l);
            if (r-l!=0) {
                if ( path_[strlen(path_)-1] != '/') {
                    strcat(path_, "/");
                }
                strcat(path_, args[0]);
                if ( access(path_, X_OK) == 0 ) {
                    if (execv(path_, args) == -1){
                        perror("Execv failed.");
                    }
                    flag = 1;
                }
            }
            l = r + 1;
            r = l;
            memset(path_, 0, sizeof(char) * 256);
            if ( flag ) {
                break;
            }
        }
        free(env_copy);
        free(path_);
    }
    if ( !flag ) {
        perror("Invalid Command");
        fflush(stdout);
    }

    exit(0);
}

/*This function handles FORK without pipe*/
void handle_fork_no_pipe(struct cmd * parsed_cmd) {
    pid_t pid;
    pid = fork(); //Create the Child
    if (pid == 0) { //If it is the child
        exec_cmd(parsed_cmd->cmd1, parsed_cmd->cmd1_fds);
        exit(0); //terminate child process.
    } else if (parsed_cmd->bg == 0) {
        waitpid(pid, NULL, 0);
    }
}

/*This function handles FORK with pipe*/
void handle_fork_with_pipe(struct cmd * parsed_cmd) {

    int fd[2];
    if (pipe(fd) == -1) {
        perror("Failed to create pipe.");
        exit(0);
    }

    pid_t pid, pid_2;
    pid = fork(); //First child created

    if (pid < 0) {
        perror("Fork 1 failed.");
        exit(0);
    }

    if (pid != 0 ) { // If it is the parent

        pid_2 = fork(); //Second child created

        if (pid_2 < 0) {
            perror("Fork 2 failed.");
            exit(0);
        }

        // 2 closes in parent, 1 close 1 dup in each child
        // Forking done --> close both ends of pipe in parent process.
        if (pid_2 != 0) {
            if ( close(fd[READ_END]) != 0   ) 
                {perror("Failed to close pipe read end from Parent.");
                exit(0);
                };
                
            if ( close(fd[WRITE_END]) != 0  ) 
                {perror("Failed to close pipe write end from Parent.");
                exit(0);
                };
                
        }


    } else { // First child

        //replace stdout of first child with write end of pipe
        if (dup2(fd[WRITE_END], WRITE_END) == -1) {
            perror("Failed to switch fds from First Child.");
            exit(0);
        }; 
        // close(fd[0]); 

        if (close(fd[READ_END]) != 0 ) {//close read end of first child
            perror("Failed to close pipe read end from  First Child.");
            exit(0);
        }

        exec_cmd(parsed_cmd->cmd1, parsed_cmd->cmd1_fds);
        exit(0); //terminate child process.
    }
    
    if (pid_2 == 0) { //Second child
        
        // replace stdin of second child with read end of pipe
        if (dup2(fd[READ_END], READ_END) == -1) {
            perror("Failed to switch fds from Second Child.");
            exit(0);
        };

        //close write end of pipe
        if (close(fd[WRITE_END]) != 0 ) {//close write end of second child
            perror("Failed to close pipe write end from Second Child.");
            exit(0);
        }
        exec_cmd(parsed_cmd->cmd2, parsed_cmd->cmd2_fds);
        exit(0); //terminate child process.

    } else if (parsed_cmd->bg == 0) {
        waitpid(pid_2, NULL, 0);
    } 
}



/*This function handles SIGCHLD*/
void sigchld_handler(int signum) {
    int status; 
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG))>0){
        // printf("signal %d me: %d child: %d\n", signum, getpid(), pid);
    }
}








/*Redundant code from CS31 (HISTORY)*/

/* The maximum size of your circular history queue. */
// #define MAXHIST 10

/*
 * A struct to keep information one command in the history of
 * command executed
 */
// struct histlist_t {
//     unsigned int cmd_num;
//     char cmdline[MAXLINE]; // command line for this process
// };

// /* Global variables declared here.
//  * For this assignment, only use globals to manage history list state.
//  * all other variables should be allocated on the stack or heap.
//  *
//  * Recall "static" means these variables are only in scope in this .c file. */
// static struct histlist_t history[MAXHIST];

// // TODO: add some more global variables for history tracking.
// //       These should be the only globals in your program.
// static int queue_next = 0; 
// static int queue_size = 0;
// unsigned int hist_ID = 0;

// void add_queue(char *val);
// void selectionSort(struct histlist_t lst[], int len);
// void print_queue(void);

// void handle_history(char ** args) {
//     // handling "HISTORY" command
//     if (strcmp(args[0], "exit") != 0){
//         hist_ID++;
//         history[queue_next].cmd_num = hist_ID;
//         add_queue(args[0]);
//     }
//     if (strcmp(args[0], "history") == 0) {
//         print_queue();
//     }
// }

// /*This function adds a value to the historty[MAXHIST] array,
// maintaining a circular queue structure.*/
// void add_queue(char *val) {

//     //if the queue is empty
//     if (queue_size == 0) {

//         queue_next = 0;
//         strcpy(history[queue_next].cmdline, val);

//         queue_next++;
//         queue_size++;
//     }

//     //if the queue is full
//     else if (queue_size == MAXHIST) {

//         strcpy(history[queue_next].cmdline, val);
//         queue_next++;
//     }

//     //if queue is neither empty nor full
//     else {

//         strcpy(history[queue_next].cmdline, val);
//         queue_size++;
//         queue_next++;
//     }

//     // if "queue_next" idx overflows, warps it back to 0
//     if (queue_next > 9) {
//         queue_next = 0;
//     }
// }



// /*This function prints out the values of history[] in first to last added order.
// In each iteration, it prints the history ID of the command, then the command itself.*/
// void print_queue(void){

//     //create new list
//     static struct histlist_t h[MAXHIST];

//     //copy all items from history to new list
//     for (int i = 0; i < queue_size; i++){
//         h[i] = history[i];
//     }

//     //sort the new list
//     selectionSort(h, queue_size);

//     //print out items in the new list in sorted order.
//     for (int i = 0; i < queue_size; i++) {
//         printf("%2d    %-20s\n", (h[i].cmd_num), (h[i].cmdline));
//     }
// }



// /*Selection Sort
// Param: the list to be sorted & its length (int).
// Function: sort the list in increasing order.
// Return: none*/
// void selectionSort(struct histlist_t lst[], int len) {

//     int min_index;
//     for (int i = 0; i < len-1; i++){
//         min_index = i;
//         for (int j = i+1; j < len; j++){
//             if (lst[j].cmd_num < lst[min_index].cmd_num){
//                 min_index = j;
//             }
//         }
//         if (min_index != i){
//             struct histlist_t temp = lst[min_index];
//             lst[min_index] = lst[i];
//             lst[i] = temp;          
//         }
//     }
// }

// void sigchld_handler(int signum);

/*This function handles SIGCHLD*/
// void sigchld_handler(int signum) {
//     int status; 
//     pid_t pid;

//     while ((pid = waitpid(-1, &status, WNOHANG))>0){
//         printf("signal %d me: %d child: %d\n", signum, getpid(), pid);
//     }
// }

// void free_2D_array(char **args) {
//     // free the contents of "args" using a loop, then free args itself.
//     int i = 0;
//     while (args[i] != NULL) {
//         free(args[i]);
//         i++;
//     }
//     free(args);
// }