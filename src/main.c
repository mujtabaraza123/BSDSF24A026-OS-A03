#include "shell.h"

int main(void) {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
        int expansion_result = expand_history_command(&cmdline);
        if (expansion_result < 0) {
            free(cmdline);
            continue;
        }

        /* Store the resolved command, so !n records the command it ran. */
        add_history_command(cmdline);

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
