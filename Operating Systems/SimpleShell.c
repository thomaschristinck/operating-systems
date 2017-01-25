#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

//Define max characters per command/line
#define MAX_LINE 75
#define MAX_JOBS 8
//Int defines position in process queue
int pos;
//Struct job holds name and process ID for each job; limited to 8 jobs at a time
struct jobs
{
  char* name;
  pid_t pid;
} jobs[MAX_JOBS];

int jobNumber;

typedef enum
{
  false,
  true
} boolean;

//Global variables to handle output redirection and piping
static boolean redirect = true;
static boolean isPipe = false;
static char *command1[10];
static char *command2[10];

void setPipe(char *args[], int *args_size){
	int i = 0; int j = 0;
	//Clear commands
	memset(command1, '\0', 10);
	memset(command2, '\0', 10);

	while(strcmp(args[i], "|") != 0){
		command1[i] = args[i];
		i++;
	}
	i++;
	while(i < *args_size){
		command2[j] = command1[i];
		i++; j++;
	}
}


// This code is given for illustration purposes. You need not include or follow this
// strictly. Feel free to writer better or bug free code. This example code block does not
// worry about deallocating memory. You need to ensure memory is allocated and deallocated
// properly so that your shell works without leaking memory. //

int getcmd(char *prompt, char *args[], int *background) {
	int length, i = 0;
	char *token, *loc, *line = NULL;
	size_t linecap = 0;


	printf("%s", prompt);
	length = getline(&line, &linecap, stdin);
	if (length <= 0) {
		exit(-1);
	} else if (length <= 1){
		puts("No command entered");
		return -1;
	}else if(line[0] == ' '){
		puts("No command entered");
		return -1;
	}
	// Check if background is specified...
	else if ((loc = index(line, '&')) != NULL) {
		*background = 1;
		*loc = ' ';
	} else
		*background = 0;


	// Check if output redirection is specified...
	if ((loc = index(line, '>')) != NULL) {
		redirect = true;
	} else{
		redirect = false;
	}

	// Check if piping is specified...
	if ((loc = index(line, '|')) != NULL) {
		isPipe = true;
	} else{
		isPipe = false;
	}
	while ((token = strsep(&line, " \t\n")) != NULL) {
		for (int j = 0; j < strlen(token); j++)
			if (token[j] <= 32)
				token[j] = '\0';
		if (strlen(token) > 0)
			args[i++] = token;
	}
	free(line);

	//For redirect symbol
	args[i] = NULL;

	if(redirect == true){
		setRedirection(args);
	}else if(isPipe == true){
		setPipe(args, &i);

	}
	return i;
}

int builtInCommands(char *args[], int *background, char **arg){
	/*
	 * cd - change directory
	 * pwd - present working directory
	 * exit - leave shell
	 * fg - move background job to foreground
	 * jobs - list background jobs
	 *
	 * Also piping and ls
	 */
	int status = 0;
	//Copy *arg to jobs list

	//Exit ("exit") - leave program
	if(strcmp(args[0],"exit") == 0){
		exit(EXIT_SUCCESS);
	}
	//Change working directory ("cd") - change working directory
	if (strcmp(args[0], "cd") == 0){
		int valid;
		valid = chdir(args[1]);
		if(valid != 0){
			perror("Error: Could not change directory.\n");
		}
	}
	//Present working directory ("pwd") - print present working directory
	else if (strcmp(args[0], "pwd") == 0) {
		char *cwd = malloc(sizeof(char) * 100);
		if (getcwd(cwd, 100)){
			fprintf(stderr, "%s\n", cwd);
	        memset(cwd, 0, sizeof(char) * 100);
		} else {
	       printf("Error getting pwd!");
	    }
	}
	//Print list of jobs ("jobs")
	else if (strcmp(args[0], "jobs") == 0) {
		boolean isJob = false;
		//We want to print a list of all non-NULL jobs
		for(int j = 0; j < jobNumber + 1; j++){
			if(jobs[j].name != NULL){
				fprintf(stderr," [%d] %s\n", j, jobs[j].name);
				isJob = true;
			}
			if (isJob == false){
				printf("No active jobs!\n");
			}
		}
	}
	else if (strcmp(args[0], "fg") == 0) {
		boolean isJob = false;
		//We want to get the requested job
		int refNumber = (int) strtol(args[1], NULL, 10);
		fprintf(stderr, "%d\n", refNumber);
		for (int i = 0 ; i <= jobNumber + 1; i++){
			//Find desired job
			if (jobs[i].name != NULL && i == refNumber) {
				isJob = true;
				printf("Moving %s [%d] %s to foreground\n", "Job ", refNumber, jobs[i].name);
				//Moved to foreground. Wait for completion. Free memory and update jobs list.
		        waitpid(jobs[i].pid, NULL, 0);
		        free(jobs[i].name);
		        jobs[i].name = NULL;
		        jobNumber --;
			}
		}
		if (isJob == false) {
			//Did not find job we were looking for
			fprintf(stderr, "No job matching ID found.\n");
		}
	}
	//Send selected job from background to foreground
	//If the command is to be executed in the child fork...
	else{
	pid_t child_pid = fork();
		if (child_pid == -1) {
	        perror("Error encountered during fork!\n");
	    } else if (child_pid ==  0) {
	    //Child process calls execvp()
	        if(execvp(args[0], args) < 0){
	        	printf("Execution failure: %s/n", strerror(errno));
	        	kill(getpid(), SIGUSR1);
	        		exit(EXIT_FAILURE);
	        	} else {
	        		exit(EXIT_SUCCESS);
	        	}
	     } else {
	    	 if(background == 0){
	    		 //If background is not specified, the parent waits on child
	    		 int waitStatus;
	    		 //Wait on child, return if no child has exited or if child has stopped
	    		 pid_t pid = waitpid(child_pid, &waitStatus, 0);
	    		 printf("Child exit with code: %d\n", WEXITSTATUS(status));
	    		 printf("Parent process, child process ID = %zu\n", child_pid);
	    	 } else {
	    		 //Otherwise setup so child runs in background
	    	 }
	    }
	}
}

int main(void) {
	char *args[20];
	//pid_t pid = getpid();
	//Background decider bg equals one if command is followed by '&'
	//(parent won't wait for child)
	int bg = 0;
	//int status = 0;
	//Initialize jobs array
	for(int i = 0; i < MAX_JOBS; i++){
		jobs[i].name = NULL;
	}
    while(1) {
        bg = 0;
        int cnt = getcmd("\n>> ", args, &bg);


    	//When no command was entered (eg. blank space)
    	if(cnt == -1)
    		continue;


       // return 0;

        //Pass args[] in child process
        /* the steps can be..:
	(1) fork a child process using fork()
	(2) the child process will invoke execvp()
	(3) if background is not specified, the parent will wait,
	otherwise parent starts the next command... */
    }
}
