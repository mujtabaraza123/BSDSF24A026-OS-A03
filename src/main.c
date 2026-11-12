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
    char **arglist;

    initialize_readline();

    while ((cmdline = readline(PROMPT)) != NULL) {
        int expansion_result = expand_history_command(&cmdline);
        if (expansion_result < 0) {
            free(cmdline);
            continue;
        }

        if (has_non_whitespace(cmdline)) {
            /* Keep both the assignment's numbered history and Readline's arrow-key history. */
            add_history_command(cmdline);
            add_history(cmdline);
        }

        arglist = tokenize(cmdline);

        if (arglist != NULL) {
            if (!handle_builtin(arglist)) {
                execute(arglist);
            }

            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
