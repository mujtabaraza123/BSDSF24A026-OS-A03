#include "shell.h"
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_IF_LINES 128

static int has_non_whitespace(const char *text) {
    if (text == NULL) return 0;
    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) return 1;
        text++;
    }
    return 0;
}

static char *trim_line(char *line) {
    while (isspace((unsigned char)*line)) line++;
    size_t length = strlen(line);
    while (length > 0 && isspace((unsigned char)line[length - 1])) {
        line[--length] = '\0';
    }
    return line;
}

/* Execute ordinary commands in a block using the same parser and executor
   as interactive commands. Returns non-zero if any foreground command fails. */
static int execute_block(const char *block) {
    char *copy = strdup(block != NULL ? block : "");
    if (copy == NULL) {
        perror("strdup");
        return 1;
    }

    int result = 0;
    char *saveptr = NULL;
    char *line = strtok_r(copy, "\n", &saveptr);
    while (line != NULL) {
        CommandList list;
        if (parse_command_list(line, &list) != 0) {
            result = 1;
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        for (int i = 0; i < list.count; i++) {
            Pipeline *pipeline = &list.pipelines[i];
            if (!list.background[i] && pipeline->command_count == 1 &&
                pipeline->commands[0].input_file == NULL &&
                pipeline->commands[0].output_file == NULL &&
                handle_builtin(pipeline->commands[0].argv)) {
                /* Built-ins execute in the shell process. */
            } else if (pipeline->command_count > 0) {
                if (execute_pipeline(pipeline, list.background[i],
                                     list.command_text[i]) != 0) result = 1;
            }
        }
        free_command_list(&list);
        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(copy);
    return result;
}

/* Read and execute a multi-line if/then/else/fi construct.
   Returns -1 for malformed/incomplete input, otherwise 0. */
static int handle_if_statement(const char *first_line) {
    char *condition = strdup(trim_line((char *)first_line) + 2);
    char *then_block = calloc(1, 1);
    char *else_block = calloc(1, 1);
    if (condition == NULL || then_block == NULL || else_block == NULL) {
        perror("allocation");
        free(condition);
        free(then_block);
        free(else_block);
        return -1;
    }

    int in_then = 0, in_else = 0, saw_else = 0, finished = 0;
    size_t then_len = 0, else_len = 0;
    char *line = NULL;
    int lines = 0;

    while ((line = readline(in_else ? "else> " : (in_then ? "then> " : "if> "))) != NULL) {
        char *trimmed = trim_line(line);
        if (strcmp(trimmed, "then") == 0 && !in_then && !in_else) {
            in_then = 1;
        } else if (strcmp(trimmed, "else") == 0 && in_then && !saw_else) {
            in_then = 0;
            in_else = 1;
            saw_else = 1;
        } else if (strcmp(trimmed, "fi") == 0 && (in_then || in_else)) {
            finished = 1;
            free(line);
            break;
        } else if (strcmp(trimmed, "if") == 0 || strncmp(trimmed, "if ", 3) == 0) {
            fprintf(stderr, "shell: nested if statements are not supported\n");
        } else if (has_non_whitespace(trimmed)) {
            if (++lines > MAX_IF_LINES) {
                fprintf(stderr, "shell: if block exceeds %d lines\n", MAX_IF_LINES);
                free(line);
                break;
            }
            char **target = in_else ? &else_block : &then_block;
            size_t *used = in_else ? &else_len : &then_len;
            size_t add = strlen(trimmed);
            char *resized = realloc(*target, *used + add + 2);
            if (resized == NULL) {
                perror("realloc");
                free(line);
                break;
            }
            *target = resized;
            memcpy(*target + *used, trimmed, add);
            *used += add;
            (*target)[(*used)++] = '\n';
            (*target)[*used] = '\0';
        }
        free(line);
    }

    if (!finished || (!in_then && !in_else)) {
        fprintf(stderr, "shell: incomplete if statement (expected then and fi)\n");
        free(condition);
        free(then_block);
        free(else_block);
        return -1;
    }

    CommandList condition_list;
    int condition_status = 1;
    if (!has_non_whitespace(condition)) {
        fprintf(stderr, "shell: missing condition after if\\n");
    } else if (parse_command_list(condition, &condition_list) == 0) {
        condition_status = 0;
        for (int i = 0; i < condition_list.count; i++) {
            Pipeline *pipeline = &condition_list.pipelines[i];
            if (pipeline->command_count == 1 &&
                pipeline->commands[0].input_file == NULL &&
                pipeline->commands[0].output_file == NULL &&
                handle_builtin(pipeline->commands[0].argv)) {
                /* A condition should normally be an external command. */
            } else if (pipeline->command_count > 0 &&
                       execute_pipeline(pipeline, condition_list.background[i],
                                        condition_list.command_text[i]) != 0) {
                condition_status = 1;
            }
        }
        free_command_list(&condition_list);
    }

    if (condition_status == 0) execute_block(then_block);
    else if (saw_else) execute_block(else_block);

    free(condition);
    free(then_block);
    free(else_block);
    return 0;
}

int main(void) {
    char *cmdline;
    initialize_readline();

    while (1) {
        reap_background_jobs();
        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;

        int expansion_result = expand_history_command(&cmdline);
        if (expansion_result < 0) {
            free(cmdline);
            continue;
        }

        if (has_non_whitespace(cmdline)) {
            add_history_command(cmdline);
            add_history(cmdline);
        }

        char *trimmed = trim_line(cmdline);
        if (strncmp(trimmed, "if ", 3) == 0) {
            handle_if_statement(trimmed);
            free(cmdline);
            continue;
        }

        CommandList list;
        if (parse_command_list(cmdline, &list) != 0) {
            free(cmdline);
            continue;
        }

        for (int i = 0; i < list.count; i++) {
            Pipeline *pipeline = &list.pipelines[i];
            if (!list.background[i] && pipeline->command_count == 1 &&
                pipeline->commands[0].input_file == NULL &&
                pipeline->commands[0].output_file == NULL &&
                handle_builtin(pipeline->commands[0].argv)) {
                /* Run state-changing built-ins in the shell process. */
            } else if (pipeline->command_count > 0) {
                execute_pipeline(pipeline, list.background[i], list.command_text[i]);
            }
        }

        free_command_list(&list);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
