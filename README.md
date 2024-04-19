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
