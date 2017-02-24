/* Key Value Store ECSE 427 Assignment1
* @author thomaschristinck
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

#include "read_write.h"

// Setup() reads the next command, separating it into distinct tokens
// using '/0' as delimiters. After setup() args should be a null-terminated string

int setup(char inputBuffer[], char *args[])
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
        case ' ' :
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
    //Buffer to hold command entered; args holds arguments
    char inputBuffer[MAX_LINE];
    char* args[MAX_LINE/2];

    char* store = "thomaschristinck1";
    kv_store_create(store);
    // Program terminates normally inside setup
    while (1)
    {
        printf(" COMMAND->\n");
        while(setup(inputBuffer, args) == 0)
        {
            //Get next command, if the setup fails to properly parse the buffer have user re-enter command
            printf(" COMMAND->\n");
        }
       

        char* str = "Greetings from shared memory! What's your name? How are you? Trying to make a message that is too long";
        int oldkey = 17;
        kv_store_write(store, oldkey, str);
        char* print = kv_store_read(store, oldkey);
        printf("Key:% d, Args 0: %s\n", oldkey, print);

         char* newstr = "Hello how are you bud?";
        int newkey = 20;
        kv_store_write(store, newkey, newstr);
        char* newprint = kv_store_read(store, newkey);
        printf("Key:% d, Value read: %s\n", newkey, newprint);

        kv_store_write(store, oldkey, newstr);
        char* lastprint = kv_store_read(store, oldkey);
          printf("Key:% d, Value read: %s\n", oldkey, lastprint);


       
    }
    return 1;
}
