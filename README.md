# Simple Shell Program

This is a simple shell program written in C that provides basic functionality similar to bash. It supports executing commands, changing directories, handling signals, and managing background processes.

## Features

- Execute shell commands
- Change current working directory
- Handle signals (SIGINT and SIGTSTP)
- Redirect input and output
- Run processes in foreground and background
- Expand $$ to process id

## Usage

To use this shell program, simply compile the `shell.c` file using a C compiler:

```bash
gcc --std=gnu99 -g smallsh.c -o smallsh
```
The run:
```bash
./shell
```
Once the shell is running, you can enter commands just like you would in a regular shell environment.
Some example commands include:

- `ls` - List files in the current directory
- `cd <directory>` - Change the current directory
- `echo <message>` - Print a message to the console
- `exit` - exit the shell

## Additional Notes
- When running a command, you can use `&` at the end to run it in the background
- Press `Ctrl + Z` to toggle foreground-only mode. In this mode, the shell will ignore `&` for background processes
- The shell provides feedback on the termination status of background processes
