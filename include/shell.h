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
#define HISTORY_SIZE 20
#define PROMPT "FCIT> "

char** tokenize(char* cmdline);
int execute(char** arglist);
int handle_builtin(char** arglist);
void initialize_readline(void);

/* Command history helpers */
void add_history_command(const char* command);
void print_command_history(void);
int expand_history_command(char** command);

#endif // SHELL_H
