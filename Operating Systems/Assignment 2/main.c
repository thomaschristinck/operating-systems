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

int setup(char inputBuffer[], char* args){
	 //Index where to place next parameter into args; number of chars entered on command line
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
	        default :
	            if (start == -1)
	                start = i;
	        }
	    }
	    //If too many chars in input
	    args[count] = NULL;
	    return 1;
}

int main(void)
{
	setup();
}

