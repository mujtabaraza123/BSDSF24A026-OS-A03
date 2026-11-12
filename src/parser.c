#include "shell.h"
#include <ctype.h>

/* Split a line into words and the special operators <, >, and |.
   Single and double quotes keep whitespace inside one argument. */
static int append_token(char **tokens, int *count, const char *start, size_t length) {
    if (*count >= 256) {
        fprintf(stderr, "shell: too many tokens\n");
        return -1;
    }

    char *token = malloc(length + 1);
    if (token == NULL) {
        perror("malloc");
        return -1;
    }
    memcpy(token, start, length);
    token[length] = '\0';
    tokens[(*count)++] = token;
    return 0;
}

static void free_tokens(char **tokens, int count) {
    for (int i = 0; i < count; i++) free(tokens[i]);
}

int parse_command_line(const char *line, Pipeline *pipeline) {
    if (pipeline == NULL || line == NULL) return -1;
    memset(pipeline, 0, sizeof(*pipeline));

    char *tokens[256];
    int token_count = 0;
    const char *p = line;

    while (*p != '\0') {
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0') break;

        if (*p == '<' || *p == '>' || *p == '|') {
            char operator_token[2] = {*p, '\0'};
            if (append_token(tokens, &token_count, operator_token, 1) != 0) {
                free_tokens(tokens, token_count);
                return -1;
            }
            p++;
            continue;
        }

        char *word = malloc(strlen(p) + 1);
        if (word == NULL) {
            perror("malloc");
            free_tokens(tokens, token_count);
            return -1;
        }
        size_t used = 0;
        char quote = '\0';

        while (*p != '\0') {
            if (quote == '\0' && (isspace((unsigned char)*p) ||
                                   *p == '<' || *p == '>' || *p == '|')) break;

            if (*p == '\\' && p[1] != '\0' && quote != '\'') {
                p++;
                word[used++] = *p++;
                continue;
            }

            if (*p == '\'' || *p == '"') {
                if (quote == '\0') {
                    quote = *p++;
                    continue;
                }
                if (quote == *p) {
                    quote = '\0';
                    p++;
                    continue;
                }
            }

            word[used++] = *p++;
        }

        if (quote != '\0') {
            fprintf(stderr, "shell: unmatched quote\n");
            free(word);
            free_tokens(tokens, token_count);
            return -1;
        }

        word[used] = '\0';
        if (append_token(tokens, &token_count, word, used) != 0) {
            free(word);
            free_tokens(tokens, token_count);
            return -1;
        }
        free(word);
    }

    if (token_count == 0) return 0;

    pipeline->command_count = 1;
    int argument_count = 0;
    ParsedCommand *current = &pipeline->commands[0];

    for (int i = 0; i < token_count; i++) {
        const char *token = tokens[i];

        if (strcmp(token, "|") == 0) {
            if (argument_count == 0 || pipeline->command_count >= MAX_COMMANDS) {
                fprintf(stderr, "shell: invalid or too many pipeline commands\n");
                free_tokens(tokens, token_count);
                free_pipeline(pipeline);
                return -1;
            }
            pipeline->command_count++;
            current = &pipeline->commands[pipeline->command_count - 1];
            argument_count = 0;
            continue;
        }

        if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0) {
            int is_input = (token[0] == '<');
            if (++i >= token_count || strcmp(tokens[i], "|") == 0 ||
                strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0) {
                fprintf(stderr, "shell: expected filename after %s\n", is_input ? "<" : ">");
                free_tokens(tokens, token_count);
                free_pipeline(pipeline);
                return -1;
            }

            char **target = is_input ? &current->input_file : &current->output_file;
            if (*target != NULL) {
                fprintf(stderr, "shell: duplicate redirection\n");
                free_tokens(tokens, token_count);
                free_pipeline(pipeline);
                return -1;
            }
            *target = strdup(tokens[i]);
            if (*target == NULL) {
                perror("strdup");
                free_tokens(tokens, token_count);
                free_pipeline(pipeline);
                return -1;
            }
            continue;
        }

        if (argument_count >= MAXARGS) {
            fprintf(stderr, "shell: maximum %d arguments per command\n", MAXARGS);
            free_tokens(tokens, token_count);
            free_pipeline(pipeline);
            return -1;
        }

        current->argv[argument_count] = strdup(token);
        if (current->argv[argument_count] == NULL) {
            perror("strdup");
            free_tokens(tokens, token_count);
            free_pipeline(pipeline);
            return -1;
        }
        argument_count++;
    }

    if (argument_count == 0) {
        fprintf(stderr, "shell: missing command in pipeline\n");
        free_tokens(tokens, token_count);
        free_pipeline(pipeline);
        return -1;
    }

    free_tokens(tokens, token_count);
    return 0;
}

/* Split a command list on unquoted ';' and '&'. An ampersand marks
   the preceding pipeline as a background job. */
int parse_command_list(const char *line, CommandList *list) {
    if (line == NULL || list == NULL) return -1;
    memset(list, 0, sizeof(*list));

    const char *start = line;
    const char *p = line;
    char quote = '\0';
    int escaped = 0;

    while (1) {
        char ch = *p;

        if (ch == '\0') {
            if (quote != '\0') {
                fprintf(stderr, "shell: unmatched quote\n");
                free_command_list(list);
                return -1;
            }
        } else if (escaped) {
            escaped = 0;
            p++;
            continue;
        } else if (ch == '\\' && quote != '\'') {
            escaped = 1;
            p++;
            continue;
        } else if (ch == '\'' || ch == '"') {
            if (quote == '\0') quote = ch;
            else if (quote == ch) quote = '\0';
            p++;
            continue;
        } else if (quote != '\0' || (ch != ';' && ch != '&')) {
            p++;
            continue;
        }

        /* At an unquoted separator or end-of-line, parse the preceding list item. */
        size_t length = (size_t)(p - start);
        char *segment = malloc(length + 1);
        if (segment == NULL) {
            perror("malloc");
            free_command_list(list);
            return -1;
        }
        memcpy(segment, start, length);
        segment[length] = '\0';

        const char *first = segment;
        while (isspace((unsigned char)*first)) first++;
        if (*first != '\0') {
            if (list->count >= MAX_LIST_COMMANDS) {
                fprintf(stderr, "shell: too many chained commands (max %d)\n",
                        MAX_LIST_COMMANDS);
                free(segment);
                free_command_list(list);
                return -1;
            }

            int index = list->count;
            if (parse_command_line(segment, &list->pipelines[index]) != 0) {
                free(segment);
                free_command_list(list);
                return -1;
            }
            list->background[index] = (ch == '&');
            list->command_text[index] = strdup(first);
            if (list->command_text[index] == NULL) {
                perror("strdup");
                free(segment);
                free_command_list(list);
                return -1;
            }
            if (list->pipelines[index].command_count > 0) list->count++;
        }
        free(segment);

        if (ch == '\0') break;
        start = p + 1;
        p++;
    }

    return 0;
}

void free_pipeline(Pipeline *pipeline) {
    if (pipeline == NULL) return;

    for (int i = 0; i < pipeline->command_count; i++) {
        ParsedCommand *command = &pipeline->commands[i];
        for (int j = 0; j < MAXARGS && command->argv[j] != NULL; j++) {
            free(command->argv[j]);
            command->argv[j] = NULL;
        }
        free(command->input_file);
        free(command->output_file);
        command->input_file = NULL;
        command->output_file = NULL;
    }
    pipeline->command_count = 0;
}

void free_command_list(CommandList *list) {
    if (list == NULL) return;
    for (int i = 0; i < list->count; i++) {
        free_pipeline(&list->pipelines[i]);
        free(list->command_text[i]);
        list->command_text[i] = NULL;
    }
    list->count = 0;
}
