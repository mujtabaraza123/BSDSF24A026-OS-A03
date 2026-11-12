#include "shell.h"
#include <ctype.h>

static char* command_history[HISTORY_SIZE];
static int history_start = 0;
static int history_count = 0;
static unsigned long history_total = 0;

char* read_cmd(char* prompt, FILE* fp) {
    char* cmdline = NULL;
    size_t capacity = 0;
    ssize_t length;

    printf("%s", prompt);
    fflush(stdout);

    length = getline(&cmdline, &capacity, fp);
    if (length == -1) {
        free(cmdline);
        return NULL;
    }

    if (length > 0 && cmdline[length - 1] == '\n') {
        cmdline[length - 1] = '\0';
    }
    return cmdline;
}

char** tokenize(char* cmdline) {
    if (cmdline == NULL) return NULL;

    char** arglist = calloc(MAXARGS + 1, sizeof(char*));
    if (arglist == NULL) {
        perror("calloc");
        return NULL;
    }

    int argnum = 0;
    char* saveptr = NULL;
    char* token = strtok_r(cmdline, " \t", &saveptr);

    while (token != NULL && argnum < MAXARGS) {
        arglist[argnum] = strdup(token);
        if (arglist[argnum] == NULL) {
            perror("strdup");
            for (int i = 0; i < argnum; i++) free(arglist[i]);
            free(arglist);
            return NULL;
        }
        argnum++;
        token = strtok_r(NULL, " \t", &saveptr);
    }

    if (token != NULL) {
        fprintf(stderr, "Error: maximum %d arguments are supported.\n", MAXARGS);
        for (int i = 0; i < argnum; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    if (argnum == 0) {
        free(arglist);
        return NULL;
    }

    return arglist;
}

void add_history_command(const char* command) {
    if (command == NULL) return;

    const unsigned char* p = (const unsigned char*)command;
    while (*p != '\0' && isspace(*p)) p++;
    if (*p == '\0') return;

    char* copy = strdup(command);
    if (copy == NULL) {
        perror("strdup");
        return;
    }

    int index;
    if (history_count < HISTORY_SIZE) {
        index = (history_start + history_count) % HISTORY_SIZE;
        history_count++;
    } else {
        index = history_start;
        free(command_history[index]);
        history_start = (history_start + 1) % HISTORY_SIZE;
    }

    command_history[index] = copy;
    history_total++;
}

void print_command_history(void) {
    unsigned long first_number = history_total - (unsigned long)history_count + 1;

    for (int i = 0; i < history_count; i++) {
        int index = (history_start + i) % HISTORY_SIZE;
        printf("%lu  %s\n", first_number + (unsigned long)i, command_history[index]);
    }
}

static const char* get_history_command(unsigned long number) {
    if (number == 0 || history_count == 0) return NULL;

    unsigned long first_number = history_total - (unsigned long)history_count + 1;
    if (number < first_number || number > history_total) return NULL;

    int offset = (int)(number - first_number);
    int index = (history_start + offset) % HISTORY_SIZE;
    return command_history[index];
}

/* Return 0 for normal input, 1 when !n was expanded, and -1 on error. */
int expand_history_command(char** command) {
    if (command == NULL || *command == NULL || (*command)[0] != '!') {
        return 0;
    }

    char* number_start = *command + 1;
    char* end = NULL;
    errno = 0;
    unsigned long number = strtoul(number_start, &end, 10);

    if (number_start == end || errno == ERANGE || number == 0) {
        fprintf(stderr, "history: invalid command number\n");
        return -1;
    }

    while (*end != '\0' && isspace((unsigned char)*end)) end++;
    if (*end != '\0') {
        fprintf(stderr, "history: invalid command number\n");
        return -1;
    }

    const char* previous = get_history_command(number);
    if (previous == NULL) {
        fprintf(stderr, "history: command %lu not found\n", number);
        return -1;
    }

    char* replacement = strdup(previous);
    if (replacement == NULL) {
        perror("strdup");
        return -1;
    }

    printf("%s\n", replacement);
    free(*command);
    *command = replacement;
    return 1;
}

int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) return 0;

    if (strcmp(arglist[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        const char* destination = arglist[1];
        if (destination == NULL) destination = getenv("HOME");

        if (destination == NULL) {
            fprintf(stderr, "cd: HOME is not set\n");
            return 1;
        }
        if (arglist[2] != NULL) {
            fprintf(stderr, "cd: too many arguments\n");
            return 1;
        }
        if (chdir(destination) != 0) {
            perror("cd");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit              Exit the shell\n");
        printf("  cd <directory>    Change the current directory\n");
        printf("  help              Display this help message\n");
        printf("  jobs              Show job-control status\n");
        printf("  history           Display recent commands\n");
        printf("  !n                Re-execute command number n\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_command_history();
        return 1;
    }

    return 0;
}
