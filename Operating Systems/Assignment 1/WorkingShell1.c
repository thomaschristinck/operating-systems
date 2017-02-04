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
#define MAX_JOBS 10

//Struct to hold job information (name and process ID)
struct job
{
    char* name;
  pid_t pid;
}
jobs[MAX_JOBS];

int jobsIndex = 0;


//Execution function (executes commands)
void exec(char* args[], int background, int out, int piping);

void exitRoutine();

void intSignalHandler(int signum){

}

void suspSignalHandler(int signum){
    printf("Got it.");
}

// Setup() reads in the next command line, separating it into distinct tokens
// using '/0' as delimiters. setup() sets args as a null-terminated string.

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
        exitRoutine();
        exit(EXIT_SUCCESS);
    }
  if (length < 0)
  {
    //Check case where length is less than 0
    perror("Error reading the command\n");
    exitRoutine();
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
        inputBuffer[i + 1] = '\t';
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

void exitRoutine(){
  for (int i= 0; i < 1 + jobsIndex; i++)
  {
    if (jobs[i].name == NULL)
    {
        free(jobs[i].name);
    }
  }

}

void piper(char *args[]){
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
        exitRoutine();
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
        wait(&pipeid);
        close(pipefd[1]);
        //Close stdin and redirect
        dup2(pipefd[0], 0);
        close(pipefd[0]);
        execvp(command2[0], command2);
    }

}

int main(void)
{
    //Buffer to hold command entered; args holds arguments
    char inputBuffer[MAX_LINE];
    char *args[MAX_LINE/2];
    //Specifies whether background, piping, output redirection was requested
    int background;
    int piping;
    int out;


    //Signal handling - ignore SIGTSTP always.
    signal(SIGTSTP, SIG_IGN);

  for (int i = 0; i < MAX_JOBS;i++)
  {
    //Initialize jobs array
    jobs[i].name=NULL;
  }
  // Program terminates normally inside setup
  while (1)
  {
    background = 0;
    out = 0;
    piping = 0;

    //Print first command prompt
    printf(" COMMAND->\n");

    while (setup(inputBuffer, args, &background, &out, &piping) == 0){
      printf(" COMMAND->\n");
    }

    for (int i = 0; i < 10; i++)
    {
      //Check the processes running in the jobs list to see if they have completed execution or not
      if (jobs[i].name != NULL)
      {
        pid_t checkPid = waitpid(jobs[i].pid, NULL, WNOHANG);
        if (checkPid == 0)
        {
          //Do nothing - child not finished process
        }
        else
        {
          //Process finished, clear spot in jobs list
          free(jobs[i].name);
          jobs[i].name = NULL;
          jobsIndex--;
        }
      }
    }
    //Execute current command
    exec(args, background, out, piping);
  }
}

//Execute command specified (background specified if background = 1)
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
    exitRoutine();
    exit(EXIT_SUCCESS);
  }
  else if(strcmp(args[0], "jobs") == 0)
  {
    //Print list of jobs ("jobs")
    int isJob = 0;
    //We want to print a list of all non-NULL jobs
    for (i= 0; i<= jobsIndex +1 ;i++)
    {
      if (jobs[i].name != NULL)
      {
        fprintf(stderr," [%d] %s\n", i, jobs[i].name);
        isJob = 1;
      }
    }
    if (isJob != 1)
    {
      printf("No active jobs\n");
    }
  }
  else if(strcmp(args[0], "fg") == 0)
  {
    //Move specified job (specified by index) to foreground
    int isJob = 0;
    //Desired job number (use strtol fr built in error handling)
    int y = (int) strtol(args[1], NULL, 10);
    fprintf(stderr, "%d\n", y);
    for (i= 0 ; i <= jobsIndex + 1; i++)
    {
      if (jobs[i].name != NULL && i == y)
      {
        isJob = 1;
        printf("Moving [%d] %s to foreground\n", y, jobs[i].name);
        //Moved to foreground. Wait for completion. Free memory and update jobs list.
        waitpid(jobs[i].pid, NULL, 0);
        free(jobs[i].name);
        jobs[i].name = NULL;
        jobsIndex--;
      }
    }
    if (!isJob)
    {
      //no matching ID for  jpb
      fprintf(stderr, "No job matching ID found.\n");
    }
  }
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
         else
        {
            //Update jobs if background was requested
            for (i= 0; i < 1 + jobsIndex; i++)
            {
                if (jobs[i].name == NULL)
                {
                    printf("background request\n");
                    jobs[i].pid = pid;
                    jobs[i].name = malloc(sizeof(char*));
                    strcpy(jobs[i].name, args[0]);
                    jobsIndex++;
                    break;
                }
            }
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
