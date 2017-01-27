/* Simple Shell ECSE 427 Assignment1

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */

struct history{        //a struct to hold the information required for a hostory feature
  char* args[MAX_LINE/2];
  int background;
}
commandHist[10];
//Should handle past 10 commands

int cmdHist = -1;

struct job            //a struct to hold the information required for a job feature
{
  char* name;
  pid_t pid;
} jobs[10]; // Lets say there wont be more jobs running than 10.

int jobsIndex = 0;

void exec(char* args[], int background);    //define a exec function for smooth execution
/**
* setup() reads in the next command line, separating it into distinct tokens
* using whitespace as delimiters. setup() sets the args parameter as a
* null-terminated string.
*/
int setup(char inputBuffer[], char *args[],int *background){
	//Number of chars in command line
	int length,
	/* loop index for accessing inputBuffer array */
  start, /* index where beginning of next command parameter is */
  ct; /* index of where to place the next parameter into args[] */

  ct = 0;

  /* read what the user enters on the command line */
  length = read(STDIN_FILENO, inputBuffer, MAX_LINE);
  start = -1;
  if (length == 0)
    exit(0); /* ^d was entered, end of user command stream */
    if (length < 0)
    {
      perror("error reading the command");
      exit(-1); /* terminate with error code of -1 */
    }
    /* examine every character in the inputBuffer */
    for (int i = 0; i < length; i++)
    {
      switch (inputBuffer[i])
    {
      case ' ':
      case '\t' : /* argument separators */
      if(start != -1)
      {
        args[ct] = &inputBuffer[start]; /* set up pointer */
        ct++;
      }
      inputBuffer[i] = '\0'; /* add a null char; make a C string */
      start = -1;
      break;
      case '\n': /* should be the final char examined */

      if (ct == 0 && start == -1) //if the user fails to enter  command before pressing enter
      {
        return 0;
      }
      if (start != -1)
      {
        args[ct] = &inputBuffer[start];
        ct++;
      }
      inputBuffer[i] = '\0';
      args[ct] = NULL; /* no more arguments to this command */
      break;
      default : /* some other character */
      if (start == -1)
        start = i;
        if (inputBuffer[i] == '&')
        {
          *background = 1;
          inputBuffer[i] = '\0';
        }
      }
    }
    args[ct] = NULL; /* just in case the input line was > 80 */
    return 1;
  }
  int main(void){
      char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
      int background; /* equals 1 if a command is followed by '&' */
      char *args[MAX_LINE/2]; /* command line (of 80) has max of 40 arguments */
      char *hold[MAX_LINE/2]; //a temp storage that mimics args for when shifting the history
      pid_t pid;
      //Initial set up for history and jobs
      commandHist[0].args[0] = NULL;  //initalize the first slot in history
      int i;
      for (int i=0; i<10;i++)
      {
        jobs[i].name=NULL;    //initalize the jobs array
        commandHist[i].background = 0; //initalize background.
      }

      while (1){
    	  // Program terminates normally inside setup
    	  background = 0;
    	  int found = 0;
    	  printf(" COMMAND->\n");
    	  while(setup(inputBuffer, args, &background) == 0){
    		  //Get next command, if the setup fails to properly parse the buffer have user re-enter command
    		  printf(" COMMAND->\n");
    	  }
    	  for (int i = 0; i < 10; i++){
    		  //Check the processes running in the jobs list to see if they have completed execution or not
    		  if (jobs[i].name != NULL){
    			  pid_t checkPid = waitpid(jobs[i].pid, NULL, WNOHANG);
    			  if (checkPid == 0){
    				  //Do nothing - child not finished process
    			  } else {
    				  jobs[i].name = NULL; //free up space in jobs list since process has finished
    				  jobsIndex--;
    			  }
    		  }
    	  }
    	  //Execute current command
    	  exec(args,background);
      }
  }

      void exec(char* args[],int background) //function that is used to execute entered command
      {
        int i;
        pid_t pid;
        if (strcmp(args[0],"cd") == 0) //If command entered is 'cd' then change directory
        {
          int ret; //used to check for valid directory
          ret = chdir(args[1]);       //If directory entered is valid change directory otherwise print error
          if (ret != 0)
            fprintf(stderr, "Error, Not a valid directory.\n");
        }
        else if(strcmp(args[0], "pwd") == 0) //If command entered is 'pwd' print current directory
        {
          fprintf(stderr, "%s\n", getcwd(NULL,(int)NULL));
        }
        else if(strcmp(args[0], "exit") == 0) //If comand entered is 'exit' exit the shell
        {
          exit(0);
        }
        else if(strcmp(args[0], "jobs") == 0) //print current jobs that are running (max 10)
        {
          int isJob = 0;
          for (i= 0; i<= jobsIndex +1 ;i++) //print all non-null jobs
          {
            if(jobs[i].name != NULL)
            {
              fprintf(stderr," [%d] %s\n", i, jobs[i].name);
              isJob = 1;
            }
          }
          if (isJob != 1)
            printf("No active jobs\n");
        }
        else if(strcmp(args[0], "fg")==0) //move selected job from the background to the foreground
        {
          int isJob = 0;
          //Invalid read with y - better allocate mem
          int y = (int) strtol(args[1], NULL, 10); //collect desired job number
          fprintf(stderr, "%d\n", y);
          for (i= 0 ; i <= jobsIndex + 1; i++)
          {
            if (jobs[i].name != NULL && i == y)
            {
                isJob = 1;
                printf("Moving [%d] %s to foreground\n", y, jobs[i].name); //once job has been moved, free memory and update jobs list
                waitpid(jobs[i].pid, NULL, 0);
                free(jobs[i].name);
                jobs[i].name = NULL;
                jobsIndex--;
            }
          }
          free(y);
          if (!isJob) //no matching ID for  jpb
          {
            fprintf(stderr, "No job matching ID found.\n");
          }
        }else {
        	//Fork a child process using fork()
        	pid = fork();
          	  if (pid == 0)
          {
            if(execvp(args[0], args) < 0){
              fprintf(stderr, "Error, Not a valid command.\n");
              exit(EXIT_FAILURE);
            } else {
              exit(EXIT_SUCCESS);
            }
            //(2) the child process will invoke execvp()
          } else if (pid > 0)/*(3) if background == 0, the parent will wait, otherwise returns to the setup() function.*/
          {
            if (background == 0)
            {
              waitpid(0, NULL, 0);
            }else
            {
              for (i= 0; i < 1 + jobsIndex; i++) //update jobs if the proces was requested to run in background
              {
                if (jobs[i].name == NULL)
                {
                  jobs[i].pid = pid;
                  jobs[i].name = strdup(args[0]);
                  jobsIndex++;
                  break;
                }
              }
            }
          } else
          {
            perror("Error: Fork failed.\n"); //else fork failed exit
            exit(-1);
          }
        }
      }
