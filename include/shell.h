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
#define MAX_LIST_COMMANDS 32
#define HISTORY_SIZE 20
#define MAX_BACKGROUND_JOBS 128
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

typedef struct {
    Pipeline pipelines[MAX_LIST_COMMANDS];
    int background[MAX_LIST_COMMANDS];
    char *command_text[MAX_LIST_COMMANDS];
    int count;
} CommandList;

char** tokenize(char* cmdline);
int parse_command_line(const char *line, Pipeline *pipeline);
int parse_command_list(const char *line, CommandList *list);
void free_pipeline(Pipeline *pipeline);
void free_command_list(CommandList *list);
int execute(char** arglist);
int execute_pipeline(Pipeline *pipeline, int background, const char *job_command);
int handle_builtin(char** arglist);
int process_variable_assignment(char **argv);
void expand_shell_variables(char **argv);
void initialize_readline(void);

/* Background job management */
void add_background_job(pid_t pid, const char *command);
void reap_background_jobs(void);
void print_background_jobs(void);

/* Command history helpers */
void add_history_command(const char* command);
void print_command_history(void);
int expand_history_command(char** command);

#endif // SHELL_H
