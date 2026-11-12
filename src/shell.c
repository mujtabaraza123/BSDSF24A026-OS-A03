#include "shell.h"
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <readline/readline.h>
#include <sys/stat.h>

static char* command_history[HISTORY_SIZE];
static int history_start = 0;
static int history_count = 0;
static unsigned long history_total = 0;

typedef struct {
    pid_t pid;
    char *command;
} BackgroundJob;

static BackgroundJob background_jobs[MAX_BACKGROUND_JOBS];
static int background_job_count = 0;

void add_background_job(pid_t pid, const char *command) {
    if (background_job_count >= MAX_BACKGROUND_JOBS) {
        fprintf(stderr, "jobs: maximum background job limit reached\n");
        return;
    }

    char *copy = strdup(command != NULL ? command : "(background command)");
    if (copy == NULL) {
        perror("strdup");
        return;
    }

    background_jobs[background_job_count].pid = pid;
    background_jobs[background_job_count].command = copy;
    background_job_count++;
}

static void remove_background_job(pid_t pid) {
    for (int i = 0; i < background_job_count; i++) {
        if (background_jobs[i].pid == pid) {
            free(background_jobs[i].command);
            for (int j = i; j < background_job_count - 1; j++) {
                background_jobs[j] = background_jobs[j + 1];
            }
            background_job_count--;
            return;
        }
    }
}

void reap_background_jobs(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_background_job(pid);
    }

    if (pid == -1 && errno != ECHILD && errno != EINTR) {
        perror("waitpid");
    }
}

void print_background_jobs(void) {
    if (background_job_count == 0) {
        printf("No background jobs.\n");
        return;
    }

    for (int i = 0; i < background_job_count; i++) {
        printf("[%d] PID %ld  %s\n", i + 1,
               (long)background_jobs[i].pid, background_jobs[i].command);
    }
}


/* Candidate storage used by GNU Readline's command-name generator. */
static char **completion_candidates = NULL;
static size_t completion_candidate_count = 0;
static size_t completion_candidate_index = 0;

static void clear_completion_candidates(void) {
    for (size_t i = 0; i < completion_candidate_count; i++) {
        free(completion_candidates[i]);
    }
    free(completion_candidates);
    completion_candidates = NULL;
    completion_candidate_count = 0;
    completion_candidate_index = 0;
}

static void add_completion_candidate(const char *name, const char *prefix) {
    if (name == NULL || prefix == NULL || strncmp(name, prefix, strlen(prefix)) != 0) {
        return;
    }

    for (size_t i = 0; i < completion_candidate_count; i++) {
        if (strcmp(completion_candidates[i], name) == 0) return;
    }

    char **resized = realloc(completion_candidates,
                             (completion_candidate_count + 1) * sizeof(char *));
    if (resized == NULL) return;
    completion_candidates = resized;

    char *copy = strdup(name);
    if (copy == NULL) return;
    completion_candidates[completion_candidate_count++] = copy;
}

static char *command_generator(const char *text, int state) {
    if (state == 0) {
        clear_completion_candidates();

        const char *builtins[] = {"cd", "exit", "help", "jobs", "history", "set", NULL};
        for (size_t i = 0; builtins[i] != NULL; i++) {
            add_completion_candidate(builtins[i], text);
        }

        const char *path_env = getenv("PATH");
        if (path_env != NULL) {
            char *path_copy = strdup(path_env);
            if (path_copy != NULL) {
                char *saveptr = NULL;
                char *directory = strtok_r(path_copy, ":", &saveptr);

                while (directory != NULL) {
                    const char *dir_name = (*directory == '\0') ? "." : directory;
                    DIR *dir = opendir(dir_name);
                    if (dir != NULL) {
                        struct dirent *entry;
                        while ((entry = readdir(dir)) != NULL) {
                            char full_path[PATH_MAX];
                            int written = snprintf(full_path, sizeof(full_path),
                                                    "%s/%s", dir_name, entry->d_name);
                            if (written > 0 && (size_t)written < sizeof(full_path) &&
                                access(full_path, X_OK) == 0) {
                                struct stat info;
                                if (stat(full_path, &info) == 0 && !S_ISDIR(info.st_mode)) {
                                    add_completion_candidate(entry->d_name, text);
                                }
                            }
                        }
                        closedir(dir);
                    }
                    directory = strtok_r(NULL, ":", &saveptr);
                }
                free(path_copy);
            }
        }
    }

    if (completion_candidate_index >= completion_candidate_count) return NULL;
    return strdup(completion_candidates[completion_candidate_index++]);
}

static char **shell_attempted_completion(const char *text, int start, int end) {
    (void)end;

    /* Complete executable and built-in names for the first word; filenames elsewhere. */
    if (start == 0 && strchr(text, '/') == NULL) {
        return rl_completion_matches(text, command_generator);
    }
    return rl_completion_matches(text, rl_filename_completion_function);
}

void initialize_readline(void) {
    rl_attempted_completion_function = shell_attempted_completion;
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


/* Shell variables are kept in the parent shell so assignments persist. */
typedef struct ShellVariable {
    char *name;
    char *value;
    struct ShellVariable *next;
} ShellVariable;

static ShellVariable *shell_variables = NULL;

static ShellVariable *find_shell_variable(const char *name) {
    for (ShellVariable *item = shell_variables; item != NULL; item = item->next) {
        if (strcmp(item->name, name) == 0) return item;
    }
    return NULL;
}

int process_variable_assignment(char **argv) {
    if (argv == NULL || argv[0] == NULL || argv[1] != NULL) return 0;
    char *equals = strchr(argv[0], '=');
    if (equals == NULL || equals == argv[0]) return 0;

    for (char *p = argv[0]; p < equals; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_') ||
            (p == argv[0] && isdigit((unsigned char)*p))) {
            return 0;
        }
    }

    size_t name_length = (size_t)(equals - argv[0]);
    char *name = strndup(argv[0], name_length);
    char *value = strdup(equals + 1);
    if (name == NULL || value == NULL) {
        perror("strdup");
        free(name);
        free(value);
        return 1;
    }

    ShellVariable *item = find_shell_variable(name);
    if (item != NULL) {
        free(item->value);
        item->value = value;
        free(name);
        return 1;
    }

    item = malloc(sizeof(*item));
    if (item == NULL) {
        perror("malloc");
        free(name);
        free(value);
        return 1;
    }
    item->name = name;
    item->value = value;
    item->next = shell_variables;
    shell_variables = item;
    return 1;
}

void expand_shell_variables(char **argv) {
    if (argv == NULL) return;
    for (int i = 0; argv[i] != NULL; i++) {
        if (argv[i][0] != '$' || argv[i][1] == '\0') continue;
        ShellVariable *item = find_shell_variable(argv[i] + 1);
        const char *value = item != NULL ? item->value : "";
        char *expanded = strdup(value);
        if (expanded == NULL) {
            perror("strdup");
            continue;
        }
        free(argv[i]);
        argv[i] = expanded;
    }
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
        printf("  set               Display shell variables\n");
        printf("  NAME=value        Assign a shell variable\n");
        printf("  echo $NAME        Expand a shell variable\n");
        printf("  set               List all shell variables\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        print_background_jobs();
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_command_history();
        return 1;
    }

    if (strcmp(arglist[0], "set") == 0) {
        for (ShellVariable *item = shell_variables; item != NULL; item = item->next) {
            printf("%s=%s\n", item->name, item->value);
        }
        return 1;
    }

    return 0;
}
