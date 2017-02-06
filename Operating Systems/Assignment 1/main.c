/* Simple Shell ECSE 427 Assignment1
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

//Max number of chars per line set here
#define MAX_LINE 80

#include "jobs.h"

//Holds the process ID of the foreground
pid_t foreground_pid;

//Execution function (executes commands)
void exec(char* args[], int background, int out, int piping);

//Signal handlers
void sig_int_handler(int sig);
void sig_susp_handler(int signum);

// Setup() reads the next command, separating it into distinct tokens
// using '/0' as delimiters. After setup() args should be a null-terminated string

int setup(char inputBuffer[], char *args[], int *background, int *out, int *piping)
{
    //Index where to place next parameter into agrs; number of chars entered on command line
    int count = 0;
    int length;
    //Index the beginning of next command parameter
    int start = -1;
    //Read what the user enters on the command line
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);

    //Input cases are the following:

    if (length == 0)
    {
        //CMD^d entered, exit successfully
        exit(EXIT_SUCCESS);
    }
    if (length < 0)
    {
        //Check case where length is less than 0
        perror("Error reading the command");
        exit(EXIT_FAILURE);
    }
    // Now examine every character in the input buffer
    for (int i = 0; i < length; i++)
    {
        switch (inputBuffer[i])
        {
        case ' ':
            //Space; do nothing
        case '\t' :
            // Argument separator
            if(start != -1)
            {
                //Set up args[] to point to the input buffer
                args[count] = &inputBuffer[start];
                count++;
            }
            // Add a null char (argument separation)
            inputBuffer[i] = '\0';
            start = -1;
            break;
        case '\n':
            // Last char examined
            if (count == 0 && start == -1)
            {
                //If the user presses enter without entering anything
                return 0;
            }
            if (start != -1)
            {
                args[count] = &inputBuffer[start];
                count++;
            }
            //No more arguments
            inputBuffer[i] = '\0';
            args[count] = NULL;
            break;
        case '>':
            *out = 1;
            inputBuffer[i] = '\0';
            break;
        case '|':
            *piping = 1;
            inputBuffer[i] = '\0';
            break;
        default :
            if (start == -1)
                start = i;
            if(inputBuffer[i] == '&')
            {
                *background = 1;
                inputBuffer[i] = '\0';
            }
            else if (inputBuffer[i] == '|')
            {
                *piping = 1;
                inputBuffer[i] = '\0';
            }
            else if (inputBuffer[i] == '>')
            {
                *out = 1;
                inputBuffer[i] = '\0';
            }
        }
    }
    //If too many chars in input
    args[count] = NULL;
    return 1;
}

//Piping function; creates a pipe and redirects output of command 1 to input of command 2
void piper(char *args[]){
    //Set up pipe
    int pipefd[2];
    pipe(pipefd);
    pid_t pipeid;
    char *command1[20];
    char *command2[20];
    //Array for first part of command
    int j = 1; 
    int k = 0; 
    command1[0] = args[0];
    command1[1] = NULL;
    //Build array for second partr of command. Looking over this before submission, I realize
    //I could have avoided this array setup proces by using execlp()
    command2[0] = NULL;
    while(args[j] != NULL)
    {
        command2[k] = args[j];
        j++;
        k++;
    }
    command2[k] = NULL;
    if((pipeid = fork()) == -1)
    {
        perror("Error while forking");
    }
    else if(!pipeid)
    {
         //Close reading end of pipe in child
        close(pipefd[0]);
        //Send stdout to pipe
        dup2(pipefd[1], 1);
        close(pipefd[1]);
        execvp(command1[0], command1);
    }
    else
    {
        //Wait for the child, and close writing end in parent
        wait(&pipeid);
        close(pipefd[1]);
        //Redirect stdin to pipe
        dup2(pipefd[0], 0);
        close(pipefd[0]);
        execvp(command2[0], command2); 
    }
  
}

int main(void)
{
    //Signal handling - ignore SIGTSTP always.
    signal(SIGTSTP, sig_susp_handler);
    signal(SIGINT, sig_int_handler);

    //Buffer to hold command entered; args holds arguments
    char inputBuffer[MAX_LINE];
    char *args[MAX_LINE/2];

    //Specifies whether background, piping, output redirection was requested
    int background;
    int piping;
    int out;

    // Program terminates normally inside setup
    while (1)
    {
        background = 0;
        out = 0;
        piping = 0;


        printf(" COMMAND->\n");

        while(setup(inputBuffer, args, &background, &out, &piping) == 0)
        {
            //Get next command, if the setup fails to properly parse the buffer have user re-enter command
            printf(" COMMAND->\n");
        }

        //Execute current command
        exec(args, background, out, piping);
    }
}

//Execute command specified (all commands executed here)
void exec(char* args[], int background, int out, int piping)
{
    int i;
    pid_t pid;
    if (strcmp(args[0],"cd") == 0)
    {
        //Change working directory ("cd") - change working directory
        //Check validity of entry
        int valid;
        valid = chdir(args[1]);
        //Print error if directory isn't valid
        if (valid != 0)
        {
            fprintf(stderr, "Error, Not a valid directory.\n");
        }
    }
    else if(strcmp(args[0], "pwd") == 0)
    {
        //Present working directory ("pwd") - print present working directory
        char *currentDir = getcwd(NULL, 0);
        fprintf(stderr, "%s\n", currentDir);
        free(currentDir);
    }
    else if(strcmp(args[0], "exit") == 0)
    {
        //Exit ("exit") - leave program
        exit(EXIT_SUCCESS);
    }
    else if(strcmp(args[0], "jobs") == 0)
        print_jobs();
    else if(strcmp(args[0], "fg") == 0)
        to_foreground(args[1]);
    else
    {
        //Fork a child process using fork()
        pid = fork();
        int fd;
        int pipefd[2];
        pipe(pipefd);
        if (pid == 0)
        {
            //CHILD
            if(out)
            {
                //Open file with specified name; if file doesn't exist, create the file. File has
                //file descriptor fd.
                if((fd = open(args[1], O_RDWR | O_CREAT, S_IRWXU)) < 0)
                {
                    printf("Couldn't open file \n");
                    exit(EXIT_FAILURE);
                }
                //Set args[1] to NULL so that only args[0] is parsed in execvp()
                args[1] = NULL;
                //Redirect output to file with file descriptor fd (opened above)
                dup2(fd, 1);
                dup2(fd, 2);
                close(fd);
            }
            else if(piping)
            {
                piper(args);
              
            }
            if(execvp(args[0], args) < 0)
            {
                fprintf(stderr, "Error, Not a valid command.\n");
                exit(EXIT_FAILURE);
            }
            else
            {
                exit(EXIT_SUCCESS);
            }
        }
        else if (pid > 0)
        {
            //PARENT
            //If background == 0, the parent will wait, otherwise returns to the setup() function.*/
            if (!background)
            {
                waitpid(pid, NULL, 0);
            }
            else if (to_background(args[0], pid) < 0)
            {
                // if to_background() failed we should wait for the child
                waitpid(0, NULL, 0);
            }
        }
        else
        {
            //Exit if fork failed with error message
            perror("Error: Fork failed.\n");
            exit(EXIT_FAILURE);
        }
    }
}

//SIGINT handler. There is a problem here. All processes in the pid group are killed (children
//in the background and foreground)
void sig_int_handler(int sig)
{
  if(foreground_pid)
  {
    kill(foreground_pid, SIGKILL);
    foreground_pid=0;
  }
 
  fflush(stdout);
}

//SIGTSTP handler. Simply ignores the signal.
void sig_susp_handler(int signum)
{
  printf("\n ****** Signal will always be ignored ****** \n");
  signal(SIGTSTP, SIG_IGN);
}
