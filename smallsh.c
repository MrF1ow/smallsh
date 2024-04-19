#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/signal.h>

#define MAX_COMMAND_LENGTH 2048
#define MAX_ARGUMENTS 512

/*-------------------------------------------------------------- Global Variables --------------------------------------------------------------*/

// pid of the current foreground process
pid_t current_foreground_pid = -5;

// exit status or termination signal of the last foreground process
int exit_method = -5;

// foreground-only mode toggle
int z_mode = 0;

// number of background processes
int num_background_process = 0;

// file to be used for input/output redirection
char *redirection_file = NULL;

/*----------------------------------------------------------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------- Function Prototypes --------------------------------------------------------------*/

void check_termination();
void catch_SIGTSTP();
void reset_SIGINT();
void deal_with_SIGTSTP();
void deal_with_SIGINT();
void deal_with_signals();
void exit_shell();
void status_command();
void change_directory(char **args, int count);
void expand_pid(char *command);
int redirection(char **args, int count, int is_in_background);
void check_background_processes();
void run_fork(char **args, int count);
void process_command(char **args, int count);
void shell_loop();

/*-------------------------------------------------------------- Signal Handlers --------------------------------------------------------------*/

/*************************************************************
 * Function: check_termination()
 * Description: Checks if the process was terminated by a signal
 * Parameters: None
 * Preconditions: None
 * Postconditions: None
 *************************************************************/
void check_termination()
{
    // check if the process was terminated by a signal
    if (WIFSIGNALED(exit_method))
    {
        // if the process was terminated by a signal, print the signal number
        printf("terminated by signal %d\n", WTERMSIG(exit_method));
        fflush(stdout);
    }
}

/*************************************************************
 * Function: catch_SIGTSTP()
 * Description: Toggles foreground-only mode
 * Parameters: None
 * Preconditions: None
 * Postconditions: Foreground-only mode is toggled
 *************************************************************/
void catch_SIGTSTP()
{
    // if not in foreground-only mode, enter foreground-only mode
    if (z_mode == 0)
    {
        z_mode = 1;
        write(STDOUT_FILENO, "\nEntering foreground-only mode (& is now ignored)\n", 53);
    }
    // if in foreground-only mode, exit foreground-only mode
    else
    {
        z_mode = 0;
        write(STDOUT_FILENO, "\nExiting foreground-only mode\n", 31);
    }
    printf(": ");
    fflush(stdout);
}

/*************************************************************
 * Function: reset_SIGINT()
 * Description: Resets SIGINT to default behavior
 * Parameters: None
 * Preconditions: None
 * Postconditions: SIGINT is reset to default behavior
 *************************************************************/
void reset_SIGINT()
{
    struct sigaction SIGINT_action = {0};

    // reset SIGINT to default behavior
    SIGINT_action.sa_handler = SIG_DFL;
    sigemptyset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;

    sigaction(SIGINT, &SIGINT_action, NULL);
}

/*************************************************************
 * Function: deal_with_SIGTSTP()
 * Description: Sets up SIGTSTP signal handler
 * Parameters: None
 * Preconditions: None
 * Postconditions: SIGTSTP signal handler is set up
 *************************************************************/
void deal_with_SIGTSTP()
{
    struct sigaction SIGTSTP_action = {0};

    // set up SIGTSTP signal handler
    SIGTSTP_action.sa_handler = catch_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    // restart system calls if interrupted by SIGTSTP
    SIGTSTP_action.sa_flags = SA_RESTART;

    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
}

/*************************************************************
 * Function: deal_with_SIGINT()
 * Description: Ignores SIGINT
 * Parameters: None
 * Preconditions: None
 * Postconditions: SIGINT is ignored
 *************************************************************/
void deal_with_SIGINT()
{
    struct sigaction SIGINT_action = {0};

    // ignore SIGINT
    SIGINT_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &SIGINT_action, NULL);
}

/*************************************************************
 * Function: deal_with_signals()
 * Description: Sets up signal handlers
 * Parameters: None
 * Preconditions: None
 * Postconditions: Signal handlers are set up
 *************************************************************/
void deal_with_signals()
{
    deal_with_SIGTSTP();
    deal_with_SIGINT();
}

/*----------------------------------------------------------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------- Built-In Shell Commands --------------------------------------------------------------*/

/*************************************************************
 * Function: exit_shell()
 * Description: Kills all processes in the group and exits the shell
 * Parameters: None
 * Preconditions: None
 * Postconditions: All processes in the group are killed and the shell exits
 *************************************************************/
void exit_shell()
{
    // get the pid of the shell process
    pid_t shell_pid = getpid();
    // get the process group id of the shell process
    pid_t group_pid = getpgid(shell_pid);

    // kill all processes in the group, including the shell
    kill(-group_pid, SIGTERM);
    exit(0);
}

/*************************************************************
 * Function: status_command()
 * Description: Prints the exit status or termination signal of the last foreground process
 * Parameters: None
 * Preconditions: None
 * Postconditions: Message is printed to stdout
 *************************************************************/
void status_command()
{
    // check if the last foreground process was terminated by a signal
    if (WIFEXITED(exit_method))
    {
        // if the last foreground process was not terminated by a signal, print the exit status
        printf("exit value %d\n", WEXITSTATUS(exit_method));
        fflush(stdout);
    }
    else
    {
        // if the last foreground process was terminated by a signal, print the signal number
        printf("terminated by signal %d\n", WTERMSIG(exit_method));
        fflush(stdout);
    }
}

/*************************************************************
 * Function: change_directory()
 * Description: Changes the current working directory
 * Parameters: char **args, int count
 * Preconditions: None
 * Postconditions: Current working directory is changed
 *************************************************************/
void change_directory(char **args, int count)
{
    // if no arguments are provided, change to the home directory
    if (count == 1)
    {
        // change to the home directory
        if (chdir(getenv("HOME")) != 0)
        {
            // if the home directory cannot be found, print an error message
            perror("Error changing directory");
            fflush(stdout);
        }
    }
    // if an argument is provided, change to the specified directory
    else
    {
        // get the current working directory
        char cwd[MAX_COMMAND_LENGTH];
        // clear the current working directory
        memset(cwd, '\0', sizeof(cwd));

        // if the argument is not an absolute path, append the current working directory to the argument
        if (args[1][0] != '/')
        {
            getcwd(cwd, sizeof(cwd));
            strcat(cwd, "/");
            strcat(cwd, args[1]);
        }
        // if the argument is an absolute path, use the argument as the new current working directory
        else
        {
            strcpy(cwd, args[1]);
        }
        // change to the new current working directory
        if (chdir(cwd) != 0)
        {
            // if the new current working directory cannot be found, print an error message
            perror("Error changing directory");
            fflush(stdout);
        }
    }
}

/*----------------------------------------------------------------------------------------------------------------------------------------------*/

/*************************************************************
 * Function: expand_pid()
 * Description: Expands $$ to the process id
 * Parameters: char *command
 * Preconditions: None
 * Postconditions: $$ is expanded to the process id
 *************************************************************/
void expand_pid(char *command)
{
    char *pos;
    // while $$ is found in the command, replace $$ with the process id
    while ((pos = strstr(command, "$$")) != NULL)
    {
        // create a buffer to hold the new command
        char buffer[MAX_COMMAND_LENGTH];
        // get the start of the command
        char *start = command;
        // get the rest of the command after $$
        char *restof = pos + 2;
        // replace $$ with the process id
        *pos = '\0';
        // create the new command
        snprintf(buffer, MAX_COMMAND_LENGTH, "%s%d%s", start, getpid(), restof);
        // copy the new command to the original command
        strncpy(command, buffer, MAX_COMMAND_LENGTH);
    }
}

/*************************************************************
 * Function: redirection()
 * Description: Redirects input and output
 * Parameters: char **args, int count, int is_in_background
 * Preconditions: None
 * Postconditions: Input and output are redirected as necessary
 *************************************************************/
int redirection(char **args, int count, int is_in_background)
{
    int input_file, output_file = -5;
    // array to store the indices of the arguments to remove
    int args_to_remove[MAX_ARGUMENTS];
    // length of the args_to_remove array, used to remove the redirection from the args
    int len_args_to_remove = 0;

    // open /dev/null for input and output redirection for background processes
    int output_null_file = open("/dev/null", O_WRONLY);
    int input_null_file = open("/dev/null", O_RDONLY);

    for (int i = 0; i < count - 1; i++)
    {
        // output redirection
        if (strcmp(args[i], ">") == 0)
        {
            // open the file for writing, create the file if it does not exist, and truncate the file to 0
            output_file = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            // if the file cannot be opened, return 1
            if (output_file == -1)
            {
                redirection_file = args[i + 1];
                return 1;
            }
            // redirect output to the file
            dup2(output_file, 1);
            close(output_file);

            // store the indices of the arguments to remove
            args_to_remove[len_args_to_remove] = i;
            len_args_to_remove++;
            args_to_remove[len_args_to_remove] = i + 1;
            len_args_to_remove++;
        }
        // input redirection
        if (strcmp(args[i], "<") == 0)
        {
            // open the file for reading
            input_file = open(args[i + 1], O_RDONLY);
            // if the file cannot be opened, return 1
            if (input_file == -1)
            {
                redirection_file = args[i + 1];
                return 1;
            }
            dup2(input_file, 0);
            close(input_file);

            // store the indices of the arguments to remove
            args_to_remove[len_args_to_remove] = i;
            len_args_to_remove++;
            args_to_remove[len_args_to_remove] = i + 1;
            len_args_to_remove++;
        }
    }

    // perform redirections for foreground processes

    // if the output file is not -5, redirect output to the file
    if (output_file != -5)
    {
        dup2(output_file, 1);
    }
    // if the input file is not -5, redirect input to the file
    if (input_file != -5)
    {
        dup2(input_file, 0);
    }

    // perform redirections for background processes

    // if the output file is -5 and the process is a background process, redirect output to /dev/null
    if (output_file == -5 && is_in_background == 1)
    {
        dup2(output_null_file, 1);
    }

    // if the input file is -5 and the process is a background process, redirect input to /dev/null
    if (input_file == -5 && is_in_background == 1)
    {
        dup2(input_null_file, 0);
    }

    // remove the redirection from the args
    for (int i = 0; i < len_args_to_remove; i++)
    {
        args[args_to_remove[i]] = NULL;
    }
    count -= len_args_to_remove;
    return 0;
}

/*************************************************************
 * Function: check_background_processes()
 * Description: Checks if any background processes have completed
 * Parameters: None
 * Preconditions: None
 * Postconditions: None
 *************************************************************/
void check_background_processes()
{
    // if there are background processes, check if any have completed
    if (num_background_process > 0)
    {
        // check if any background processes have completed
        pid_t completed_process = waitpid(-1, &exit_method, WNOHANG);
        // if a background process has completed, print the exit status or termination signal
        if (completed_process > 0)
        {
            printf("background pid %d is done: ", completed_process);
            fflush(stdout);

            // if the process was not terminated by a signal, print the exit status
            if (WIFEXITED(exit_method))
            {
                printf("exit value %d\n", WEXITSTATUS(exit_method));
                fflush(stdout);
            }
            // if the process was terminated by a signal, print the signal number
            else
            {
                printf("terminated by signal %d\n", WTERMSIG(exit_method));
                fflush(stdout);
            }
        }
    }
}

/*************************************************************
 * Function: run_fork()
 * Description: Forks a child process and runs the command
 * Parameters: char **args, int count, char* fullCommand
 * Preconditions: None
 * Postconditions: Child process is forked and command is run
 *************************************************************/
void run_fork(char **args, int count)
{
    pid_t spawnPid = fork();
    int redirect_output_status = -5;
    int background_command = 0;

    // if the last argument is &, remove it and set background_command to 1
    if (strcmp(args[count - 1], "&") == 0)
    {
        if (!z_mode)
        {
            background_command = 1;
        }
        args[count - 1] = NULL;
        count--;
    }

    switch (spawnPid)
    {
    // fork failed
    case -1:
        perror("fork() failed!");
        exit(1);
        break;
    // Child Process
    case 0:
        // if it is not a background process, set the current foreground pid and reset SIGINT
        if (!background_command)
        {
            current_foreground_pid = getpid();
            reset_SIGINT();
        }

        // checked and performed redirection
        redirect_output_status = redirection(args, count, background_command);
        // if redirection failed, print an error message and exit
        if (redirect_output_status == 1)
        {
            printf("bash: %s: No such file or directory\n", redirection_file);
            fflush(stdout);
            exit(1);
        }

        // execute the command
        execvp(args[0], args);

        // if the command cannot be executed, print an error message and exit
        fprintf(stderr, "bash: %s: command not found\n", args[0]);
        exit(1);
        break;
    // Parent Process
    default:
        // if it is not a background process, wait for the child process to complete
        if (!background_command)
        {
            current_foreground_pid = spawnPid;

            spawnPid = waitpid(spawnPid, &exit_method, 0);

            // check if the process was terminated by a signal
            check_termination();
        }
        // if it is a background process, print the pid of the background process
        else
        {
            printf("Background pid is %d\n", spawnPid);
            fflush(stdout);

            // increment the number of background processes
            num_background_process++;
        }
        break;
    };
}

/*************************************************************
 * Function: process_command()
 * Description: Processes the command
 * Parameters: char **args, int count, char* fullCommand
 * Preconditions: None
 * Postconditions: Command is processed accordingly
 *************************************************************/
void process_command(char **args, int count)
{
    // if the command is empty or a comment, return
    if (args[0][0] == '#' || args[0][0] == '\n')
    {
        return;
    }

    // remove newline from the last argument
    size_t len = strlen(args[count - 1]);
    if (len > 0 && args[count - 1][len - 1] == '\n')
    {
        args[count - 1][len - 1] = '\0';
    }

    // if the command is "exit", exit the shell
    if (strcmp(args[0], "exit") == 0)
    {
        exit_shell();
    }
    // if the command is "status", print the exit status or termination signal of the last foreground process
    else if (strcmp(args[0], "status") == 0)
    {
        status_command();
    }
    // if the command is "cd", change the current working directory
    else if (strcmp(args[0], "cd") == 0)
    {
        change_directory(args, count);
    }
    // if the command is a not a built-in command, run the command in a child process
    else
    {
        run_fork(args, count);
    }
}

/*************************************************************
 * Function: shell_loop()
 * Description: Main shell loop
 * Parameters: None
 * Preconditions: None
 * Postconditions: Shell loop is run
 *************************************************************/
void shell_loop()
{
    while (1)
    {
        // periodically check child/background processes
        check_background_processes();

        // print the prompt
        printf(": ");
        fflush(stdout);

        // get the command from the user
        char *fullCommand = NULL;
        char **arguments = malloc(MAX_ARGUMENTS * sizeof(char *));
        size_t bufferSize = 0;
        int counter = 0;

        getline(&fullCommand, &bufferSize, stdin);

        // expand $$ to pid
        expand_pid(fullCommand);

        // parse command into arguments
        char *token = strtok(fullCommand, " ");
        while (token != NULL)
        {
            arguments[counter] = malloc(strlen(token) + 1);
            strcpy(arguments[counter], token);
            counter++;
            token = strtok(NULL, " ");
        }
        arguments[counter] = NULL;

        // process command accordingly
        process_command(arguments, counter);
    }
}

int main()
{
    // set up signal handlers
    deal_with_signals();
    // run the shell loop
    shell_loop();
    return 0;
}
