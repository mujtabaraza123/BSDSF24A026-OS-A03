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

## Command Chaining and Background Jobs

- Separate commands with `;` to run them sequentially.
- End a command with `&` to run it in the background without waiting.
- Use `jobs` to list active background processes and their PIDs.
- Completed background children are reaped with non-blocking `waitpid(-1, ..., WNOHANG)` before the next prompt.

Examples:

```bash
echo "First command" ; sleep 2 ; echo "Second command"
sleep 10 &
jobs
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

## Conditional if-then-else-fi

The shell supports multi-line conditional blocks. The command after `if` runs first; an exit status of 0 runs the `then` block, while a non-zero status runs the optional `else` block. Finish the structure with `fi`.

Example:

```bash
if grep "user" /etc/passwd > /dev/null
then
  echo "Found user"
else
  echo "User not found"
fi
```

An `else` block is optional:

```bash
if [ -f "myfile.txt" ]
then
  echo "myfile.txt exists."
fi
```


## Shell Variables

- Assign a value with `NAME=value` (do not put spaces around `=`).
- Use `$NAME` in a command to expand the stored value.
- Use `set` to display all variables currently defined in this shell.

Example:

```bash
MSG="hello"
set
echo $MSG
MSG="Good morning"
echo $MSG
```
