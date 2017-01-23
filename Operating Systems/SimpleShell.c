#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

//Keep track of position on stack, job list.
int pos; jobs;

//
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
	}
	// Check if background is specified..
	if ((loc = index(line, '&')) != NULL) {
		*background = 1;
		*loc = ' ';
	} else
		*background = 0;
	while ((token = strsep(&line, " \t\n")) != NULL) {
		for (int j = 0; j < strlen(token); j++)
			if (token[j] <= 32)
				token[j] = '\0';
		if (strlen(token) > 0)
			args[i++] = token;
	}
	return i;
}

int builtInCommands(char *args[], int count){
	/*
	 * cd - change directory
	 * pwd - present working directory
	 * exit - leave shell
	 * fg - foreground a background job
	 * jobs - list background jobs
	 *
	 * Also piping and ls
	 */
	int status = 0;
	int continueStatus = 0;
	char cwd[pathconf(".", _PC_PATH_MAX)];

	 if (strcmp(args[0], "cd") != 0){
		 if (count == 1)
			 args[1] = getenv("HOME");
		 status = chdir(args[1]);
		 if (status == -1)
			 //Error - requested file or directory doesn't exist
			 printf("cd: \'%s\': No such file or directory\n", args[1]);
		pos--;
	 }

	 else if (strcmp(args[0], "pwd") == 0) {
		if (count != 1)
		    printf("pwd: Too many args");
		else {
		    if (getcwd(cwd, pathconf(".", _PC_PATH_MAX)) == cwd)
		    	printf("%s\n", cwd);
		    else {
		    	printf("pwd: Error getting pwd");
		    	pos--;
		    }
		}
	 }
	 return continueStatus;
}

int main(void) {
	char *args[20];
	pid_t pid = getpid();
	int bg;
	int proc;
    while(1) {
        bg = 0;
        int cnt = getcmd("\n>> ", args, &bg);

        if (cnt < 0)
        	    continue;

        	proc = builtInCommands(args, cnt);
        	if (proc == 0)
        	    continue;
        	else if (proc == 1){
        	    if (!builtInCommands(args, cnt))
        	    	continue;
        	}

        pid_t ret_pid = fork();
        if (ret_pid == (pid_t) -1) {
        	printf("Error encountered during fork!\n");
        } else if (ret_pid == (pid_t) 0) {
        	printf("child process\n");
        	if(execvp(args[0], args) < 0){
        		printf("Execution failure: %s/n", strerror(errno));
        		kill(pid, SIGUSR1);
        		exit(EXIT_FAILURE);
        	} else {
        		exit(EXIT_SUCCESS);
        	}
        } else  {
        	printf("Parent process, child process ID = %zu\n",
                      (size_t) ret_pid);
        }

       // return 0;

        //Pass args[] in child process
        /* the steps can be..:
	(1) fork a child process using fork()
	(2) the child process will invoke execvp()
	(3) if background is not specified, the parent will wait,
	otherwise parent starts the next command... */
    }
}
