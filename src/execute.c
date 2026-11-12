#include "shell.h"
#include <fcntl.h>

static void close_pipe_fds(int pipes[][2], int count) {
    for (int i = 0; i < count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

int execute(char* arglist[]) {
    int status;
    pid_t cpid = fork();

    if (cpid == -1) {
        perror("fork");
        return -1;
    }

    if (cpid == 0) {
        execvp(arglist[0], arglist);
        perror("Command not found");
        _exit(EXIT_FAILURE);
    }

    while (waitpid(cpid, &status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return -1;
        }
    }
    return 0;
}

int execute_pipeline(Pipeline *pipeline) {
    if (pipeline == NULL || pipeline->command_count <= 0) return 0;

    int command_count = pipeline->command_count;
    int pipes[MAX_COMMANDS - 1][2];
    pid_t children[MAX_COMMANDS];

    for (int i = 0; i < command_count - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            close_pipe_fds(pipes, i);
            return -1;
        }
    }

    int started = 0;
    for (int i = 0; i < command_count; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close_pipe_fds(pipes, command_count - 1);
            for (int j = 0; j < started; j++) {
                while (waitpid(children[j], NULL, 0) == -1 && errno == EINTR) {}
            }
            return -1;
        }

        if (pid == 0) {
            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                perror("dup2 stdin");
                _exit(EXIT_FAILURE);
            }
            if (i < command_count - 1 && dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                perror("dup2 stdout");
                _exit(EXIT_FAILURE);
            }

            close_pipe_fds(pipes, command_count - 1);

            ParsedCommand *command = &pipeline->commands[i];
            if (command->input_file != NULL) {
                int fd = open(command->input_file, O_RDONLY);
                if (fd == -1) {
                    perror(command->input_file);
                    _exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("dup2 input");
                    close(fd);
                    _exit(EXIT_FAILURE);
                }
                close(fd);
            }

            if (command->output_file != NULL) {
                int fd = open(command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror(command->output_file);
                    _exit(EXIT_FAILURE);
                }
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2 output");
                    close(fd);
                    _exit(EXIT_FAILURE);
                }
                close(fd);
            }

            execvp(command->argv[0], command->argv);
            perror(command->argv[0]);
            _exit(EXIT_FAILURE);
        }

        children[started++] = pid;
    }

    close_pipe_fds(pipes, command_count - 1);

    int result = 0;
    for (int i = 0; i < started; i++) {
        int status = 0;
        int waited;
        do {
            waited = (int)waitpid(children[i], &status, 0);
        } while (waited == -1 && errno == EINTR);

        if (waited == -1) {
            perror("waitpid");
            result = -1;
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            result = -1;
        }
    }
    return result;
}
