/*

 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "parsecmd.h"

static void print_cmd_info(char *cmdline, struct cmd * parsed_cmd);
static void print_bg(int bg) ;
static void print_args(char **argv);
static void print_fds(char **fds);

int main(int argc, char **argv) {

    int bg;
    char cmdline[MAXLINE];
    struct cmd * parsed_cmd = NULL;

    printf("Type \"quit\" or \"exit\" to stop.\n");
    while (1) {
        // Print a prompt
        printf("enter a cmd line> ");
        fflush(stdout);

        // Read in the next command
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
            perror("fgets error");
        }

        // Check for end of file (ctrl-d)
        if (feof(stdin)) {
            printf("\n");
            fflush(stdout);
            exit(0);
        }

        // Exit on command
        if (!strncmp(cmdline, "quit", 4) || !strncmp(cmdline, "exit", 4)) {
            fflush(stdout);
            exit(0);
        }

        // Initialize bg
        bg = 0;
        parsed_cmd = parse_cmd_dynamic(cmdline, &bg);
        print_cmd_info(cmdline, parsed_cmd);
        free_cmd(parsed_cmd);
    }

    return 0;
}

/* This function prints out a message based on the value of bg
   indicating if the command is run in the background or not. */
void print_bg(int bg) {
    if (bg) {
        printf("The process is running in background. \n");
    } else {
        printf("The process is NOT running in background. \n");
    }
}

/* This function prints out the arguments of a command
*/
void print_args(char **argv) {
    int i = 0;
    printf("Arguments are: \n");
    while (argv[i] != NULL) {
        // NOTE: Print each argv string between '#' characters so that we
        // can see if there is any space or other invisible characters in the
        // argv[i] string (there shouldn't be).
        printf("%3d  #%s#\n", i, argv[i]);
        i++;
    }
}


/*  This function prints the fds information
*/
void print_fds(char **fds) {
    if (fds[0]!= NULL) {
        printf("stdin is \"%s\"\n", fds[0]);
    } else {
        printf("stdin is NULL\n");
    }

    if (fds[1]!= NULL) {
        printf("stdout is \"%s\"\n", fds[1]);
    } else {
        printf("stdout is NULL\n");
    }

    if (fds[2]!= NULL) {
        printf("stderr is \"%s\"\n", fds[2]);
    } else {
        printf("stderr is NULL\n");
    }
}

/* This function prints out the cmdline and its parsed information
 * cmdline: the command line string.
 * parsed_cmd: pointer to the parsed command. */
void print_cmd_info(char *cmdline, struct cmd * parsed_cmd) {

    printf("\nCommand line: %s\n", cmdline);

    print_bg(parsed_cmd -> bg);

    //  Print out information of first command 
    printf("------Command 1 Info---------\n");
    print_args(parsed_cmd -> cmd1);
    print_fds(parsed_cmd -> cmd1_fds);

    if (parsed_cmd -> has_pipe) { 
        // Print out the information of second command
        printf("------Command 2 Info---------\n");
        print_args(parsed_cmd -> cmd2);
        print_fds(parsed_cmd -> cmd2_fds);
    }
}
