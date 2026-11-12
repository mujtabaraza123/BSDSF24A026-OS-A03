#include "shell.h"
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

static int has_non_whitespace(const char *text) {
    if (text == NULL) return 0;
    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) return 1;
        text++;
    }
    return 0;
}

int main(void) {
    char *cmdline;

    initialize_readline();

    while ((cmdline = readline(PROMPT)) != NULL) {
        int expansion_result = expand_history_command(&cmdline);
        if (expansion_result < 0) {
            free(cmdline);
            continue;
        }

        if (has_non_whitespace(cmdline)) {
            add_history_command(cmdline);
            add_history(cmdline);
        }

        Pipeline pipeline;
        if (parse_command_line(cmdline, &pipeline) != 0) {
            free(cmdline);
            continue;
        }

        if (pipeline.command_count == 1 &&
            pipeline.commands[0].input_file == NULL &&
            pipeline.commands[0].output_file == NULL &&
            handle_builtin(pipeline.commands[0].argv)) {
            /* Built-ins run in the shell process when no redirection is requested. */
        } else if (pipeline.command_count > 0) {
            execute_pipeline(&pipeline);
        }

        free_pipeline(&pipeline);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
