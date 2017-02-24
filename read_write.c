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
#define POD_NUMBERS 256
#define VALUE_SIZE 30  
#define POD_SIZE (VALUE_SIZE * POD_LENGTH)

//The store will have the following values at each position in each pod: (1) An address, (2) A boolean
//specifying if that slot is empty, and (3) a queue position, with position POD_LENGTH -1 being the head.

typedef struct Store
{
	char* addr[POD_NUMBERS][POD_LENGTH];
	bool is_empty[POD_NUMBERS][POD_LENGTH];
	int queue_pos[POD_NUMBERS][POD_LENGTH];

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
	//Loop through all sections and set to is_empty = true
	for (int i = 0; i < POD_NUMBERS; i++)
	{
		for(int j = 0; j < POD_LENGTH; j++)
		{
			store.is_empty[i][j] = true;
			store.addr[i][j] = "\0";
			store.queue_pos[i][j] = 0;
		}
	}
	return 1;
}

int kv_store_write(char* name, int key, char* value)
{
	if (sizeof(value) > VALUE_SIZE)
	{
		printf("String value to be stored is too long! Max is %d bytes!", VALUE_SIZE);
		return -1;
	}
	//Open the shared memory object
	int fd = shm_open(name, O_RDWR, S_IRWXU);
	if (fd == -1)
	{
		printf("Error opening shared memory object: %s", strerror(errno));
		return -1;
	}

	/*//Map file to memory, save address of the mapped data. Then set up all
	//pods of size POD_SIZE 
	store.addr[0][0] = mmap(NULL, VALUE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	for(int i = 0; i < POD_NUMBERS; i++)
	{
		for(int j = 0; j < POD_LENGTH; j++)
		{
			if (j != 0 || i != 0)
			{
				//Set up all addresses
				store.addr[i][j] = (void*) ((char*)store.addr[i][0] + (j + 1) * VALUE_SIZE);
			}
		}
	}*/
	store.addr[0][0] = mmap(NULL, VALUE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	for(int i = 0; i < POD_NUMBERS; i++)
	{
		store.addr[i][0] = (void*) ((char*)store.addr[i-1][0] + POD_SIZE);
		for(int j = 0; j < POD_LENGTH; j++)
		{
			if (j != 0 || i != 0)
			{
				//Set up all addresses
				store.addr[i][j] = (void*) ((char*)store.addr[i][0] + (j + 1) * VALUE_SIZE);
			}
		}
	}
	//Size the store to fit pods
	ftruncate(fd, POD_LENGTH * POD_NUMBERS * VALUE_SIZE);
	close(fd);

	//Hash the key to determine pod to store data
	int pod_number = key % POD_NUMBERS;

	//Idea - when key is known we know what pod, i.e pod[i], to write in. We can divide this pod into POD_SIZE / VALUE_SIZE sections.
	//Maybe set up an array of pods, where each pod is a queue of values
	
	//Now look for the first non-empty slot. If all slots are empty, evict the FIRST (HOW TO DO??) value entered
	//Loop through all sections and set to is_empty = true
	/*for(int j = 0; j < POD_LENGTH; j++)
	{
		if (store.is_empty[pod_number][j])
		{
			store.is_empty[pod_number][j] = false;
			//Copy string to memory
			memcpy(store.addr[pod_number][j], value, strlen(value));
			return 1;
		}
	}*/
	//If this is reached, we need to evict the first entered slot
	/*
	1 (FIRST)
	2
	3
	4
	5 (LAST)
	*/
	store.is_empty[pod_number][0] = false;
	memcpy(store.addr[pod_number][0], value, strlen(value));
	//memset(store[pod_number][0].addr, '\0', VALUE_SIZE);
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
	int pod_number = key % POD_NUMBERS;
	//Map file to memory, save address of the mapped data. Then set up all
	//pods of size POD_SIZE 
	store.addr[0][0] = mmap(NULL, POD_LENGTH * POD_NUMBERS * VALUE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	for(int i = 0; i < POD_NUMBERS; i++)
	{
		char* temp = store.addr[i][0];
		for(int j = 0; j < POD_LENGTH; j++)
		{
			if (j != 0 || i != 0)
			{
				//Set up all addresses
				store.addr[i][j] = temp + (j + 1) * VALUE_SIZE;
			}
		}
	}
	for(int k = 0; k < POD_LENGTH; k++)
	{
		if(!store.is_empty[pod_number][k])
			//Return value stored in pod, indexed by the key
			return store.addr[pod_number][k];
	}

	//Otherwise return Null for failure
	return NULL;
	
}

char** kv_store_read_all(char* name, int key)
{
	char* tempc = "yes";
	char** tempcp = &tempc;
	return tempcp;
}

int hash (char* key)
{
	//Some hashing function
}