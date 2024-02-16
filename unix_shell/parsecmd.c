#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parsecmd.h"





void free_cmd(struct cmd * parsed_cmd);
void copyFds(int index, int fdsId, char ** fds, char ** tokens);
void copyTokens(char ** tokens, char *** dest);
int identifyToken(char **tokens, const char * target) ;
char *slice_string(const char *str, int start, int end);
int count_num_tokens(char ** tokens);
int get_ampersand_idx(const char *str, int *bg);
int get_num_tokens(const char *cmdline);
int get_next_non_space(const char *str, int current_index);
int get_next_space(const char *str, int current_index);
void free_args(char **args);
void free_args_2(char **args);




struct cmd * parse_cmd_dynamic(const char *cmdline, int *bg) {
    char **tokens = NULL;
    const char *  PIPE = "|";
    const char * STDIN = "<";
    const char * STDOUT = "1>";
    const char * STDERR = "2>";

    int pipe_idx;
    int has_pipe;

    int stdin_idx;
    int stdout_idx;
    int stderr_idx;
    *bg = 0;

    int index_of_and = get_ampersand_idx(cmdline, bg); // get the index of the firsrt '&'

    char *valid_cmd = strndup(cmdline, index_of_and); // duplicate the substring without '&'

    int num_tokens = get_num_tokens(valid_cmd); // get the number of tokens

    int num_tokens_cmd1;
    int num_tokens_cmd2;
    int i, tok_start, tok_end;


    tokens = malloc(sizeof(char*) * ( num_tokens + 1 ) );

    struct cmd * parsed_cmd = malloc(sizeof(struct cmd));

    parsed_cmd->bg = *bg;
    parsed_cmd->has_pipe = 0;
    parsed_cmd -> cmd1 = NULL;
    parsed_cmd -> cmd2 = NULL;
    for ( i = 0 ; i < 3; i ++ ) {
        parsed_cmd -> cmd1_fds[i] = NULL;
        parsed_cmd -> cmd2_fds[i] = NULL;
    }

    tok_start = 0;
    tok_end = 0;
    for ( i = 0; i < num_tokens; i ++ ) { // add each token to tokens
        tok_start = get_next_non_space(valid_cmd, tok_end);
        tok_end = get_next_space(valid_cmd, tok_start);
        tokens[ i ] = slice_string(valid_cmd, tok_start, tok_end); 
    }
    tokens[ num_tokens ] = NULL; // append NULL to the end of arguments
    
    free(valid_cmd);

    // identify if there is a pipe in the command
    pipe_idx = identifyToken(tokens, PIPE);
    if ( pipe_idx == -1 ) { // if there is no pipe
        has_pipe = 0;
        parsed_cmd -> has_pipe = 0;

        stdin_idx = identifyToken(tokens, STDIN);  
        copyFds(stdin_idx, 0, parsed_cmd -> cmd1_fds, tokens);

        stdout_idx = identifyToken(tokens, STDOUT);
        copyFds(stdout_idx, 1, parsed_cmd -> cmd1_fds, tokens);
        
        stderr_idx = identifyToken(tokens, STDERR);
        copyFds(stderr_idx, 2, parsed_cmd -> cmd1_fds, tokens);

        copyTokens(tokens, &(parsed_cmd -> cmd1));
        free_args(tokens);
        
    } else { // if there is a pipe
        has_pipe = 1;
        parsed_cmd -> has_pipe = 1;

        free(tokens[pipe_idx]);
        tokens[pipe_idx] = NULL;

        copyTokens(tokens, &(parsed_cmd -> cmd1));
        copyTokens(tokens+pipe_idx+1, &(parsed_cmd -> cmd2));

        free_args_2(tokens+pipe_idx+1);
        free_args(tokens);

        stdin_idx = identifyToken(parsed_cmd -> cmd1, STDIN);  
        copyFds(stdin_idx, 0, parsed_cmd -> cmd1_fds, parsed_cmd -> cmd1);
        stdout_idx = identifyToken(parsed_cmd -> cmd1, STDOUT);
        copyFds(stdout_idx, 1, parsed_cmd -> cmd1_fds, parsed_cmd -> cmd1);
        stderr_idx = identifyToken(parsed_cmd -> cmd1, STDERR);
        copyFds(stderr_idx, 2, parsed_cmd -> cmd1_fds, parsed_cmd -> cmd1);

        stdin_idx = identifyToken(parsed_cmd -> cmd2, STDIN);  
        copyFds(stdin_idx, 0, parsed_cmd -> cmd2_fds, parsed_cmd -> cmd2);
        stdout_idx = identifyToken(parsed_cmd -> cmd2, STDOUT);
        copyFds(stdout_idx, 1, parsed_cmd -> cmd2_fds, parsed_cmd -> cmd2);
        stderr_idx = identifyToken(parsed_cmd -> cmd2, STDERR);
        copyFds(stderr_idx, 2, parsed_cmd -> cmd2_fds, parsed_cmd -> cmd2);
    }
    
    return parsed_cmd;
}

/**********HELPER FUNCTIONS FOR parse_cmd_dynamic**********/

/*
    free_args frees dynamically allocated 2D array.
 */
void free_args(char **args) {
    int index = 0;
    while ( args[index] != NULL ) {
        free(args[index]);
        index ++;
    }
    free(args);
}

void free_args_2(char **args) {
    int index = 0;
    while ( args[index] != NULL ) {
        free(args[index]);
        index ++;
    }
}

/*
    get_next_non_space
    get the index of the next non space after current index.
    If there is no next non space, return the index of the 
    NULL terminator. 
*/
int get_next_non_space(const char *str, int current_index) {

    //ret is the index of the next non space
    int ret = current_index;
    while ( isspace(str[ret]) ) {
        ret ++;
        if ( str[ret] == '\0' ) { // whether is the end of string
            break;
        }
    }
    return ret;
}

/*
    get the index of the next space after current index.
    If there is no next space, return the index of the 
    NULL terminator. 
*/
int get_next_space(const char *str, int current_index) {
    /*
        ret is the index of the next space
    */
    int ret = current_index;
    while ( !isspace(str[ret]) ) {
        ret ++;
        if ( str[ret] == '\0' ) { // if is the end of string
            break;
        }
    }
    return ret;
}

/*
    get_num_tokens counts the number of tokens in a command
    line without '&'. Return the number of tokens
*/
int get_num_tokens(const char *cmdline) {
    /*
        cnt is the count of number of tokens. 
        index is the current index of the command line.
    */
    int cnt = 0; 
    int index = 0; 
    while ( cmdline[ index ] != '\0' ) { // iterate until it is end of string
        index = get_next_non_space(cmdline, index); // get the start index of next token
        if ( cmdline[ index ] == '\0' ) { // if there is no next token
            break;
        }
        cnt ++; 
        index = get_next_space(cmdline, index); // the the index of next white space
    }
    return cnt;
}

/*
    cnt is the count of number of tokens. 
    index is the current index of the command line.
*/
int count_num_tokens(char ** tokens) {
    int cnt = 0; 
    int index = 0; 
    while ( tokens[ index ] != NULL ) { // iterate until it is end of string
        cnt ++;
        index ++;
    }
    return cnt;
}

/*
    slice_string creates a substring of str from index start
    to index end. Return the pointer to the substrng
*/
char *slice_string(const char *str, int start, int end) {
    int len = end-start; // get the length of substring
    char *ret = malloc(sizeof(char) * (len+1)); 
    int i;
    for ( i = 0 ; i < len; i ++ ) { 
        ret[ i ] = str[ start + i ]; // copy each char from the main string to the substring
    }
    ret[ len ] = '\0'; // append the null terminator
    return ret;
}

/*
    get_ampersand_idx finds the index of first '&' in the string.
    If there is, return the index, and set the flag bg to 1.
    If there is no, return the length of the string
*/
int get_ampersand_idx(const char *str, int *bg) {
    int len = strlen(str);
    int i;
    for ( i = 0; i < len; i ++ ) {
        if (str[i] == '&') { // if there is an '&';
            *bg = 1;
            return i;
        }
    }
    return len;
}

/*
    identity a specific token
    return the index of the token if there exists, 
    return -1 otherwise
*/
int identifyToken(char **tokens, const char * target) {
    for (int i = 0; ;i++ ) {
        if (tokens[i] == NULL) {    
            break;
        }
        if (strcmp(tokens[i], target)==0 ) { // if the token is the same
            return i;
        }
    }
    return -1; // return -1 if there is no stdin
}

/* 
    copy tokens from one array of tokens to another
*/
void copyTokens(char ** tokens, char *** dest) {
    int num_tokens = count_num_tokens(tokens);
    int i;
    if (*dest != NULL) {
        free(*dest);
    }
    *dest = malloc(sizeof(char *) * (num_tokens+1) );
    for ( i = 0 ; i < num_tokens; i ++ ) {
        (*dest)[i] = strdup(*(tokens+i));
    }
    (*dest)[num_tokens] = NULL;
}

/* 
    copy the fds destination and remove the destination token 
    from the original token list 
*/
void copyFds(int index, int fdsId, char ** fds, char ** tokens) {
    if (index >= 0 ) {
        fds[fdsId] = strdup(tokens[index+1]);
        free(tokens[index]);
        free(tokens[index+1]);
        // shift the remaining tokens to the left
        int i = index;
        while ( tokens[i+2] != NULL) {
            tokens[i] = tokens[i+2];
            i ++;
        }
        tokens[i] = NULL;
        tokens[i+1] = NULL;
    } else {
        fds[fdsId] = NULL;
    }
}

/*
    free the space allocated to cmd
*/
void free_cmd(struct cmd * parsed_cmd) {
    int i;
    free_args(parsed_cmd -> cmd1);
    if ( parsed_cmd -> has_pipe) {
        free_args(parsed_cmd -> cmd2); 
    } 
    for ( i = 0 ; i < 3; i ++ ) {
        if (parsed_cmd -> cmd1_fds[i]!= NULL) {
            free(parsed_cmd -> cmd1_fds[i]);
        }
        if (parsed_cmd -> cmd2_fds[i]!= NULL) {
            free(parsed_cmd -> cmd2_fds[i]);
        }
    }
    free(parsed_cmd);
}