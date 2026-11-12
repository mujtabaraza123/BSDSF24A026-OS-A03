# PUFCIT Custom UNIX Shell

A simple command-line interpreter (shell) built in C for an Operating Systems course.

## Dependencies

GNU Readline development library is required.

On Ubuntu/WSL:

```bash
sudo apt update
sudo apt install libreadline-dev
```

## Build

```bash
make
```

## Run

```bash
./bin/myshell
```

## Readline Features

- Use Up/Down arrow keys to navigate command history.
- Press Tab on the first word to complete built-in and executable command names from PATH.
- Press Tab after the first word to complete filenames and paths.
- The `history` command lists the most recent 20 commands.
- `!n` re-executes a numbered command from the history list.

## I/O Redirection and Pipes

- `command > file` redirects standard output to a file.
- `command < file` reads standard input from a file.
- `command1 | command2` sends the first command's output to the second command.
- Quoted arguments such as `grep "root"` are kept together by the parser.

Examples:

```bash
ls -l > file_list.txt
sort < file_list.txt
cat /etc/passwd | grep "root"
```

## Clean

```bash
make clean
```

## Directory Structure

- `src/`: C source files.
- `include/`: Header files.
- `bin/`: Compiled shell executable (`myshell`).
- `obj/`: Intermediate object files.
- `Makefile`: Build instructions.
