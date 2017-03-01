#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <errno.h>
#include <unistd.h>

#include "read_write.h"


#define POD_LENGTH 6
#define POD_NUMBERS 128
#define VALUE_SIZE 30  
#define POD_SIZE (VALUE_SIZE * POD_LENGTH)

typedef struct Store
{
	char* addr[POD_NUMBERS];

} Store;

Store store;

int kv_store_create(char* name)
{
	//Establish connection between file descriptor and shared memory object
	int fd = shm_open(name, O_CREAT|O_RDWR, S_IRWXU);
	if (fd == -1)
	{
		printf("Error opening shared memory object: %s", strerror(errno));
		return -1;
	}
	return 1;
	//Shared memory should be cleaned here?
}

//To remove name field for every method, maybe write an "open" function which opens the file with name 
//and returns the file descriptor
int kv_store_write(char* name, int key, char* value)
{
	if (strlen(value) > VALUE_SIZE)
	{
		printf("String value to be stored is too long! Max is 30 characters!");
		return -1;
	}
	//Open the shared memory object
	int fd = shm_open(name, O_RDWR, S_IRWXU);
	if (fd == -1)
	{
		printf("Error opening shared memory object: %s", strerror(errno));
		return -1;
	}

	//Map file to memory, save pointer to the start of the mapped data. Then set up all
	//pods of size POD_SIZE 
	store.addr[0] = mmap(NULL, VALUE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	for(int i = 1; i < POD_NUMBERS; i++)
	{
		store.addr[i] = (void*) ((char*)store.addr[i-1] + POD_SIZE);
	}
	
	//Hash the key to determine pod to store data
	int pod = key % POD_NUMBERS;
	
	//Size the store to fit POD_NUMBERS pods
	ftruncate(fd, POD_NUMBERS * POD_SIZE);
	close(fd);

	//Copy string to memory
	memcpy(store.addr[pod], value, strlen(value));
	return 1;
}

char* kv_store_read(char* name, int key)
{
	//Open the shared memory object
	int fd = shm_open(name, O_RDONLY, 0);
	if (fd == -1)
	{
		printf("Error opening shared memory object: %s", strerror(errno));
		return NULL;
	}
	//Find pod associated with key
	int pod = key % POD_NUMBERS;
	store.addr[0] = mmap(NULL, VALUE_SIZE, PROT_READ, MAP_SHARED, fd, 0);
	for(int i = 1; i < POD_NUMBERS; i++)
	{
		store.addr[i] = (void*) ((char*)store.addr[i-1] + POD_SIZE);
	}
	//Return value stored in pod, indexed by the key
	return store.addr[pod];
	
}

char** kv_store_read_all(char* name, int key)
{
	char* tempc = "yes";
	char** tempcp = &tempc;
	return tempcp;
}
