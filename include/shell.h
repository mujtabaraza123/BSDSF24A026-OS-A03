#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAXARGS 10
#define MAX_COMMANDS 10
#define HISTORY_SIZE 20
#define PROMPT "FCIT> "

typedef struct {
    char *argv[MAXARGS + 1];
    char *input_file;
    char *output_file;
} ParsedCommand;

typedef struct {
    ParsedCommand commands[MAX_COMMANDS];
    int command_count;
} Pipeline;

char** tokenize(char* cmdline);
int parse_command_line(const char *line, Pipeline *pipeline);
void free_pipeline(Pipeline *pipeline);
int execute(char** arglist);
int execute_pipeline(Pipeline *pipeline);
int handle_builtin(char** arglist);
void initialize_readline(void);

/* Command history helpers */
void add_history_command(const char* command);
void print_command_history(void);
int expand_history_command(char** command);

#endif // SHELL_H
