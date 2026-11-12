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

    while (1) {
        /* Reap completed background children before displaying the next prompt. */
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
                execute_pipeline(pipeline, list.background[i], cmdline);
            }
        }

        free_command_list(&list);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
