#include "shell.h"

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
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    return 0;
}
