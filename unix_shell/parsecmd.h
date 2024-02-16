#ifndef _PARSECMD__H_
#define _PARSECMD__H_

#define MAXLINE    1024   // max command line size
#define MAXARGS     128   // max number of arguments on a command line

/*
*    cmdline: The command line string entered at the shell prompt
*
*         bg: Pointer to an int. Set to 1 if cmd runs in background, 0 otherwise
*
*    returns: struct cmd, as defined below.
*/

struct cmd {
    int has_pipe; // boolean value, 1 if there are two commands
    int bg; // boolean value, 1 if it runs in background
    char ** cmd1;
    char ** cmd2;
    char * cmd1_fds[3];
    char * cmd2_fds[3];
};

struct cmd * parse_cmd_dynamic(const char *cmdline, int *bg);
void free_cmd(struct cmd * parsed_cmd);

#endif