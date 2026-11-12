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
